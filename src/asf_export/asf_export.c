/*==================BEGIN ASF AUTO-GENERATED DOCUMENTATION==================*/
/*
ABOUT EDITING THIS DOCUMENTATION:
If you wish to edit the documentation for this program, you need to change the
following defines. For the short ones (like ASF_NAME_STRING) this is no big
deal. However, for some of the longer ones, such as ASF_COPYRIGHT_STRING, it
can be a daunting task to get all the newlines in correctly, etc. In order to
help you with this task, there is a tool, edit_man_header. The tool *only*
works with this portion of the code, so fear not. It will scan in defines of
the format #define ASF_<something>_STRING between the two auto-generated
documentation markers, format them for a text editor, run that editor, allow
you to edit the text in a clean manner, and then automatically generate these
defines, formatted appropriately. The only warning is that any text between
those two markers and not part of one of those defines will not be preserved,
and that all of this auto-generated code will be at the top of the source
file. Save yourself the time and trouble, and use edit_man_header. :)
*/

#define ASF_NAME_STRING \
"asf_export"

#define ASF_USAGE_STRING \
"   "ASF_NAME_STRING" [-format <output_format>] [-size <max_dimension>]\n"\
"              [-byte <sample mapping option>] [-log <log_file>] [-quiet]\n"\
"              [-license] [-version] [-help]\n"\
"              <in_base_name> <out_full_name>\n"

#define ASF_DESCRIPTION_STRING \
"   This program ingests ASF internal format data and exports said data to a\n"\
"   number of imagery formats. If the input data was geocoded and the ouput\n"\
"   format supports geocoding, that information will be included.\n"

#define ASF_INPUT_STRING \
"   A file set in the ASF internal data format.\n"

#define ASF_OUTPUT_STRING \
"   The converted data in the output file.\n"

#define ASF_OPTIONS_STRING \
"   -format <format>\n"\
"        Format to export to. Must be one of the following:\n"\
"            tiff    - Tagged Image File Format, with byte valued pixels\n"\
"            geotiff - GeoTIFF file, with floating point or byte valued pixels\n"\
"            jpeg    - Lossy compressed image, with byte valued pixels\n"\
"            ppm     - Portable pixmap image, with byte valued pixels\n"\
"   -size <size>\n"\
"        Scale image so that its largest dimension is, at most, <size>.\n"\
"   -byte <sample mapping option>\n"\
"        Converts output image to byte using the following options:\n"\
"            truncate\n"\
"                truncates the input values regardless of their value range.\n"\
"            minmax\n"\
"                determines the minimum and maximum values of the input image\n"\
"                and linearly maps those values to the byte range of 0 to 255.\n"\
"            sigma\n"\
"                determines the mean and standard deviation of an image and\n"\
"                applies a buffer of 2 sigma around the mean value (it\n"\
"                adjusts this buffer if the 2 sigma buffer is outside the\n"\
"                value range).\n"\
"            histogram_equalize\n"\
"                produces an image with equally distributed brightness levels\n"\
"                over the entire brightness scale which increases contrast.\n"\
"   -log <logFile>\n"\
"        Output will be written to a specified log file.\n"\
"   -quiet\n"\
"        Supresses all non-essential output.\n"\
"   -license\n"\
"        Print copyright and license for this software then exit.\n"\
"   -version\n"\
"        Print version and copyright then exit.\n"\
"   -help\n"\
"        Print a help page and exit.\n"

#define ASF_EXAMPLES_STRING \
"   To export to the default geotiff format from file1.img and file1.meta\n"\
"   to file1.tif:\n"\
"        example> "ASF_NAME_STRING" file1 file1\n"\
"\n"\
"   To export to file2.jpg in the jpeg format:\n"\
"        example> "ASF_NAME_STRING" -format jpeg file1 file2\n"\
"\n"\
"   To export file1 to a jpeg called file3.jpg no larger than 800x800:\n"\
"        example> "ASF_NAME_STRING" -format jpeg -size 800 file1 file3\n"

#define ASF_LIMITATIONS_STRING \
"   Currently only supports ingest of ASF format floating point data.\n"\
"\n"\
"   Floating-point image formats (i.e., geotiff) are not supported in many\n"\
"   image viewing programs.\n"

#define ASF_SEE_ALSO_STRING \
"   asf_convert, asf_import\n"

/*===================END ASF AUTO-GENERATED DOCUMENTATION===================*/


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
#include <gsl/gsl_math.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_statistics.h>

#include <asf.h>
#include <asf_endian.h>
#include <asf_meta.h>
#include <asf_export.h>
#include <asf_contact.h>
#include <asf_license.h>

// Print minimalistic usage info & exit
static void print_usage(void)
{
  asfPrintStatus("\n"
      "Usage:\n"
      ASF_USAGE_STRING
      "\n");
  exit(EXIT_FAILURE);
}

// Print the help info & exit
static void print_help(void)
{
  asfPrintStatus(
      "\n"
      "Tool name:\n   " ASF_NAME_STRING "\n\n"
      "Usage:\n" ASF_USAGE_STRING "\n"
      "Description:\n" ASF_DESCRIPTION_STRING "\n"
      "Input:\n" ASF_INPUT_STRING "\n"
      "Output:\n"ASF_OUTPUT_STRING "\n"
      "Options:\n" ASF_OPTIONS_STRING "\n"
      "Examples:\n" ASF_EXAMPLES_STRING "\n"
      "Limitations:\n" ASF_LIMITATIONS_STRING "\n"
      "See also:\n" ASF_SEE_ALSO_STRING "\n"
      "Contact:\n" ASF_CONTACT_STRING "\n"
      "Version:\n   " CONVERT_PACKAGE_VERSION_STRING "\n\n");
  exit(EXIT_SUCCESS);
}

int
checkForOption (char *key, int argc, char *argv[])
{
  int ii = 0;
  while ( ii < argc ) {
    if ( strmatch (key, argv[ii]) )
      return ii;
    ++ii;
  }
  return FLAG_NOT_SET;
}


// Check the return value of a function and display an error message
// if it's a bad return.
void
check_return (int ret, char *msg)
{
  if ( ret != 0 )
    asfPrintError (msg);
}


// Main program body.
int
main (int argc, char *argv[])
{
  output_format_t format = 0;
  meta_parameters *md;
  char *in_base_name;

  in_base_name = (char *) MALLOC(sizeof(char)*255);

/**********************BEGIN COMMAND LINE PARSING STUFF**********************/
  // Command line input goes in it's own structure.
  command_line_parameters_t command_line;
  strcpy (command_line.format, "");
  command_line.size = NO_MAXIMUM_OUTPUT_SIZE;
  strcpy (command_line.in_data_name, "");
  strcpy (command_line.in_meta_name, "");
  strcpy (command_line.output_name, "");
  command_line.verbose = FALSE;
  command_line.quiet = FALSE;
  strcpy (command_line.leader_name, "");
  strcpy (command_line.cal_params_file, "");
  strcpy (command_line.cal_comment, "");
  command_line.sample_mapping = 0;

  int formatFlag, sizeFlag, logFlag, quietFlag, byteFlag;
  int needed_args = 3;  //command & argument & argument
  int ii;
  char sample_mapping_string[25];

  //Check to see which options were specified
  if (   (checkForOption("--help", argc, argv) != FLAG_NOT_SET)
      || (checkForOption("-h", argc, argv) != FLAG_NOT_SET)
      || (checkForOption("-help", argc, argv) != FLAG_NOT_SET) ) {
      print_help();
  }
  handle_license_and_version_args(argc, argv, ASF_NAME_STRING);

  formatFlag = checkForOption ("-format", argc, argv);
  sizeFlag = checkForOption ("-size", argc, argv);
  logFlag = checkForOption ("-log", argc, argv);
  quietFlag = checkForOption ("-quiet", argc, argv);
  byteFlag = checkForOption ("-byte", argc, argv);

  if ( formatFlag != FLAG_NOT_SET ) {
    needed_args += 2;           // Option & parameter.
  }
  if ( sizeFlag != FLAG_NOT_SET ) {
    needed_args += 2;           // Option & parameter.
  }
  if ( quietFlag != FLAG_NOT_SET ) {
    needed_args += 1;           // Option & parameter.
  }
  if ( logFlag != FLAG_NOT_SET ) {
    needed_args += 2;           // Option & parameter.
  }
  if ( byteFlag != FLAG_NOT_SET ) {
    needed_args += 2;           // Option & parameter.
  }

  if ( argc != needed_args ) {
    print_usage ();                   // This exits with a failure.
  }

  // We also need to make sure the last three options are close to
  // what we expect.
  if ( argv[argc - 1][0] == '-' || argv[argc - 2][0] == '-' ) {
    print_usage (); // This exits with a failure.
  }

  // Make sure any options that have parameters are followed by
  // parameters (and not other options) Also make sure options'
  // parameters don't bleed into required arguments.
  if ( formatFlag != FLAG_NOT_SET ) {
    if ( argv[formatFlag + 1][0] == '-' || formatFlag >= argc - 3 ) {
      print_usage ();
    }
  }
  if ( sizeFlag != FLAG_NOT_SET ) {
    if ( argv[sizeFlag + 1][0] == '-' || sizeFlag >= argc - 3 ) {
      print_usage ();
    }
  }
  if ( byteFlag != FLAG_NOT_SET ) {
    if ( argv[byteFlag + 1][0] == '-' || byteFlag >= argc -3 ) {
      print_usage ();
    }
  }
  if ( logFlag != FLAG_NOT_SET ) {
    if ( argv[logFlag + 1][0] == '-' || logFlag >= argc - 3 ) {
      print_usage ();
    }
  }

  if( logFlag != FLAG_NOT_SET ) {
    strcpy(logFile, argv[logFlag+1]);
  }
  else {
    sprintf(logFile, "tmp%i.log", (int)getpid());
  }
  logflag = TRUE; // Since we always log, set the old school logflag to true
  fLog = FOPEN (logFile, "a");

  // Set old school quiet flag (for use in our libraries)
  quietflag = ( quietFlag != FLAG_NOT_SET ) ? TRUE : FALSE;

  // We're good enough at this point... print the splash screen.
  asfSplashScreen (argc, argv);

  // Set default output type
  if( formatFlag != FLAG_NOT_SET ) {
    strcpy (command_line.format, argv[formatFlag + 1]);
  }
  else {
    // Default behavior: produce a geotiff.
    strcpy (command_line.format, "geotiff");
  }

  // Convert the string to upper case.
  for ( ii = 0 ; ii < strlen (command_line.format) ; ++ii ) {
    command_line.format[ii] = toupper (command_line.format[ii]);
  }

  // Set the default byte scaling mechanisms
  if ( strcmp (command_line.format, "TIFF") == 0
       || strcmp (command_line.format, "TIF") == 0
       || strcmp (command_line.format, "JPEG") == 0
       || strcmp (command_line.format, "JPG") == 0
       || strcmp (command_line.format, "PPM") == 0)
    command_line.sample_mapping = SIGMA;
  else if ( strcmp (command_line.format, "GEOTIFF") == 0 )
    command_line.sample_mapping = NONE;

  if ( sizeFlag != FLAG_NOT_SET )
    command_line.size = atol (argv[sizeFlag + 1]);
  else
    command_line.size = NO_MAXIMUM_OUTPUT_SIZE;

  if ( quietFlag != FLAG_NOT_SET )
    command_line.quiet = TRUE;
  else
    command_line.quiet = FALSE;

  // Set scaling mechanism
  if ( byteFlag != FLAG_NOT_SET ) {
    strcpy (sample_mapping_string, argv[byteFlag + 1]);
    for ( ii = 0; ii < strlen(sample_mapping_string); ii++) {
      sample_mapping_string[ii] = toupper (sample_mapping_string[ii]);
    }
    if ( strcmp (sample_mapping_string, "TRUNCATE") == 0 )
      command_line.sample_mapping = TRUNCATE;
    else if ( strcmp(sample_mapping_string, "MINMAX") == 0 )
      command_line.sample_mapping = MINMAX;
    else if ( strcmp(sample_mapping_string, "SIGMA") == 0 )
      command_line.sample_mapping = SIGMA;
    else if ( strcmp(sample_mapping_string, "HISTOGRAM_EQUALIZE") == 0 )
      command_line.sample_mapping = HISTOGRAM_EQUALIZE;
    else
      asfPrintError("Unrecognized byte scaling method '%s'.\n",
                    sample_mapping_string);
  }

  //Grab the input name
  strcpy (in_base_name, argv[argc - 2]);
  strcpy (command_line.in_meta_name, in_base_name);
  //Grab the output name
  strcpy (command_line.output_name, argv[argc - 1]);
  printf("in_base_name: %s\n", in_base_name);
  printf("output_name: %s\n", command_line.output_name);

/***********************END COMMAND LINE PARSING STUFF***********************/

  if ( strcmp (command_line.format, "ENVI") == 0 ) {
    format = ENVI;
  }
  else if ( strcmp (command_line.format, "ESRI") == 0 ) {
    format = ESRI;
  }
  else if ( strcmp (command_line.format, "GEOTIFF") == 0 ||
            strcmp (command_line.format, "GEOTIF") == 0) {
    append_ext_if_needed (command_line.output_name, ".tif", ".tiff");
    format = GEOTIFF;
  }
  else if ( strcmp (command_line.format, "TIFF") == 0 ||
            strcmp (command_line.format, "TIF") == 0) {
    append_ext_if_needed (command_line.output_name, ".tif", ".tiff");
    format = TIF;
  }
  else if ( strcmp (command_line.format, "JPEG") == 0 ||
            strcmp (command_line.format, "JPG") == 0) {
    append_ext_if_needed (command_line.output_name, ".jpg", ".jpeg");
    format = JPEG;
  }
  else if ( strcmp (command_line.format, "PPM") == 0 ) {
    append_ext_if_needed (command_line.output_name, ".ppm", NULL);
    format = PPM;
  }
  else {
    asfPrintError("Unrecognized output format specified");
  }

  /* Complex data generally can't be output into meaningful images, so
     we refuse to deal with it.  */
  md = meta_read (command_line.in_meta_name);
  asfRequire (   md->general->data_type == BYTE
              || md->general->data_type == INTEGER16
              || md->general->data_type == INTEGER32
              || md->general->data_type == REAL32
              || md->general->data_type == REAL64,
              "Cannot cope with complex data, exiting...\n");
  meta_free (md);

  // Do that exporting magic!
  asf_export(format, command_line.size, 
	     command_line.sample_mapping, in_base_name,
	     command_line.output_name);

  // If the user didn't ask for a log file then nuke the one that's been kept
  // since everything has finished successfully
  if (logFlag == FLAG_NOT_SET) {
      fclose (fLog);
      remove(logFile);
  }

  exit (EXIT_SUCCESS);
}
