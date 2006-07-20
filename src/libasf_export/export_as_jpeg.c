#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include <cla.h>
#include <envi.h>
#include <esri.h>
#include <geokeys.h>
#include <geotiff.h>
#include <geotiffio.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_statistics.h>
#include <jpeglib.h>

#include <asf.h>
#include <asf_endian.h>
#include <asf_meta.h>
#include <asf_export.h>
#include <asf_raster.h>

void
export_as_jpeg (const char *metadata_file_name,
                const char *image_data_file_name, const char *output_file_name,
                long max_size, scale_t sample_mapping)
{
  /* Get the image metadata.  */
  meta_parameters *md = meta_read (metadata_file_name);
  /* Scale factor needed to satisfy max_size argument.  */
  size_t scale_factor;
  int jj;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *ofp;

  asfRequire (md->general->data_type == REAL32,
              "Input data type must be in big endian 32-bit floating point "
              "format.\n");

  /* Get the image data.  */
  asfPrintStatus ("Loading image...\n");
  const off_t start_of_file_offset = 0;
  FloatImage *iim
    = float_image_new_from_file (md->general->sample_count,
                                 md->general->line_count,
                                 image_data_file_name, start_of_file_offset,
                                 FLOAT_IMAGE_BYTE_ORDER_BIG_ENDIAN);

  /* We want to scale the image st the long dimension is less than or
     equal to the prescribed maximum, if any.  */
  if ( (max_size > iim->size_x && max_size > iim->size_y)
       || max_size == NO_MAXIMUM_OUTPUT_SIZE ) {
    scale_factor = 1;
  }
  else {
    scale_factor = ceil ((double) GSL_MAX (iim->size_x, iim->size_y)
                         / max_size);
    /* The scaling code we intend to use needs odd scale factors.  */
    if ( scale_factor % 2 != 1 ) {
      scale_factor++;
    }
  }

  /* Generate the scaled version of the image, if needed.  */
  FloatImage *si;
  if ( scale_factor != 1 ) {
    asfPrintStatus ("Scaling...\n");
    si = float_image_new_from_model_scaled (iim, scale_factor);
  }
  else {
    si = iim;
  }

  /* We might someday want to mask out certain valus for some type of
     images, so they don't corrupt the statistics used for mapping
     floats to JSAMPLEs.  Eanbling this will require changes to the
     statistics routines and the code that does the mapping from
     floats to JSAMPLEs.  */
  /*
   * double mask;
   * if ( md->general->image_data_type == SIGMA_IMAGE
   *      || md->general->image_data_type == GAMMA_IMAGE
   *      || md->general->image_data_type == BETA_IMAGE
   *      || strcmp(md->general->mode, "SNA") == 0.0
   *      || strcmp(md->general->mode, "SNB") == 0.0
   *      || strcmp(md->general->mode, "SWA") == 0.0
   *      || strcmp(md->general->mode, "SWB") == 0.0 )
   *   mask = 0.0;
   * else
   *   mask = NAN;
   */

  /* We need a version of the data in JSAMPLE form, so we have to map
     floats into JSAMPLEs.  We do this by defining a region 2 sigma on
     either side of the mean to be mapped in the range of JSAMPLE
     linearly, and clamping everything outside this range at the
     limits o the JSAMPLE range.  */
  /* Here are some very funky checks to try to ensure that the JSAMPLE
     really is the type we expect, so we can scale properly.  */
  asfRequire(sizeof(unsigned char) == 1,
             "Size of the unsigned char data type on this machine is "
             "different than expected.\n");
  asfRequire(sizeof(unsigned char) == sizeof (JSAMPLE),
             "Size of unsigned char data type on this machine is different "
             "than JPEG byte size.\n");
  JSAMPLE test_jsample = 0;
  test_jsample--;
  asfRequire(test_jsample == UCHAR_MAX,
             "Something wacky happened, like data overflow.\n");

  /* Gather some statistics to help with the mapping.  Note that if
     min_sample and max_sample will actually get used for anything
     they will be set to some better values than this.  */
  asfPrintStatus("Gathering image statistics...\n");
  float min_sample = -1.0, max_sample = -1.0;
  float mean, standard_deviation;
  const int default_sampling_stride = 30;
  const int minimum_samples_in_direction = 10;
  int sampling_stride = GSL_MIN (default_sampling_stride,
                                 GSL_MIN (si->size_x, si->size_y)
                                 / minimum_samples_in_direction);
  /* Lower and upper extents of the range of float values which are to
     be mapped linearly into the output space.  */
  float omin, omax;
  gsl_histogram *my_hist = NULL;
  get_statistics (si, sample_mapping, sampling_stride, &mean,
                  &standard_deviation, &min_sample, &max_sample, &omin, &omax,
                  &my_hist, md->general->no_data);

  asfPrintStatus("Writing Output File...\n");

  /* Initializae libjpg structures.  */
  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_compress (&cinfo);

  /* Open the output file to be used.  */
  ofp = fopen (output_file_name, "w");
  if ( ofp == NULL ) {
    asfPrintError("Open of %s for writing failed: %s",
                  output_file_name, strerror(errno));
  }

  /* Connect jpeg output to the output file to be used.  */
  jpeg_stdio_dest (&cinfo, ofp);

  /* Set image parameters that libjpeg needs to know about.  */
  cinfo.image_width = si->size_x;
  cinfo.image_height = si->size_y;
  cinfo.input_components = 1;   /* Grey scale => 1 color component / pixel.  */
  cinfo.in_color_space = JCS_GRAYSCALE;
  jpeg_set_defaults (&cinfo);   /* Use default compression parameters.  */
  /* Reassure libjpeg that we will be writing a complete JPEG file.  */
  jpeg_start_compress (&cinfo, TRUE);

  // Stuff for histogram equalization
  gsl_histogram_pdf *my_hist_pdf = NULL;
  if ( sample_mapping == HISTOGRAM_EQUALIZE ) {
    my_hist_pdf = gsl_histogram_pdf_alloc (NUM_HIST_BINS);
    gsl_histogram_pdf_init (my_hist_pdf, my_hist);
  }

  /* Write the jpeg.  */
  float *float_row = g_new (float, si->size_x);
  JSAMPLE *jsample_row = g_new (JSAMPLE, si->size_x);
  while ( cinfo.next_scanline < cinfo.image_height ) {
    /* We are writing one row at a time.  */
    const int rows_to_write = 1;
    int rows_written;
    JSAMPROW *row_pointer = MALLOC (rows_to_write * sizeof (JSAMPROW));
    float_image_get_row (si, cinfo.next_scanline, float_row);
    for ( jj = 0 ; jj < si->size_x ; jj++ ) {
      jsample_row[jj] = (JSAMPLE) pixel_float2byte(float_row[jj],
                                                   sample_mapping, omin, omax,
                                                   my_hist, my_hist_pdf,
                                                   md->general->no_data);
    }
    row_pointer[0] = jsample_row;
    rows_written = jpeg_write_scanlines (&cinfo, row_pointer, rows_to_write);
    asfRequire(rows_written == rows_to_write,
               "Failed to write the correct number of lines.\n");
    FREE (row_pointer);
    asfLineMeter(cinfo.next_scanline, cinfo.image_height);
  }

  /* Finsh compression and close the jpeg.  */
  jpeg_finish_compress (&cinfo);
  FCLOSE (ofp);
  jpeg_destroy_compress (&cinfo);

  g_free (jsample_row);
  g_free (float_row);

  // If the scale factor wasn't one, the scaled version of the image
  // will be different from the original and so will need to be freed
  // seperately.
  if ( si != iim) {
    float_image_free (si);
  }

  float_image_free (iim);

  meta_free (md);
}

