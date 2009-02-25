/****************************************************************
FUNCTION NAME:  meta_get_*

DESCRIPTION:
   Extract relevant parameters from CEOS.
   These are the external entry points for
   the geolocation-related routines.

RETURN VALUE:

SPECIAL CONSIDERATIONS:

PROGRAM HISTORY:
  1.0 - O. Lawlor.  9/10/98.  CEOS Independance.
****************************************************************/
#include <assert.h>
#include "asf.h"
#include "asf_meta.h"
#include <libasf_proj.h>
//#include "jpl_proj.h"

/*******************************************************************
 * Prototypes                                                     */
double *get_a_coeffs(meta_parameters *meta);
double *get_b_coeffs(meta_parameters *meta);
int is_valid_map2s_transform(meta_parameters *meta);
int is_valid_map2l_transform(meta_parameters *meta);
int is_valid_ll2l_transform(meta_parameters *meta);
int is_valid_ll2s_transform(meta_parameters *meta);

/*Geolocation Calls:*/

/*******************************************************************
 * meta_get_original_line_sample:
 * Converts a given line and sample to that for the non-multilooked,
 * non-windowed, equivalent image.  This function shows how
 * gracelessly our metadata represents the different types of images
 * we have to cope with. */
void meta_get_original_line_sample(meta_parameters *meta,
        int line, int sample,
        int *original_line, int *original_sample)
{
  *original_line   = line   * meta->sar->line_increment
        + meta->general->start_line;
  *original_sample = sample * meta->sar->sample_increment
        + meta->general->start_sample;
}

/******************************************************************
 * meta_get_orig:
 * DEPRECATED.  You probably want meta_get_original_line_sample.  */
void meta_get_orig(void *fake_ddr, int y, int x,int *yOrig,int *xOrig)
{
  struct DDR *ddr=(struct DDR *)fake_ddr;
  *xOrig=(x)*ddr->sample_inc + (ddr->master_sample-1);
  *yOrig=(y)*ddr->line_inc + (ddr->master_line-1);
}

/*******************************************************************
 * meta_get_latLon:
 * Converts given line and sample to geodetic latitude and longitude.
 * Works with all image types.*/
int meta_get_latLon(meta_parameters *meta,
                    double yLine, double xSample,double elev,
                    double *lat,double *lon)
{
  double hgt;

  if (meta->projection) {
    /*Map-Projected. Use projection information to calculate lat & lon.*/
    double px,py,pz=0.0;
    px = meta->projection->startX + ((xSample + meta->general->start_sample)
             * meta->projection->perX);
    py = meta->projection->startY + ((yLine + meta->general->start_line)
             * meta->projection->perY);
    /*Currently have to handle scansar separately, because it depends on
      a lot more info than the other projections */
    if (meta->projection->type == SCANSAR_PROJECTION) {
        scan_to_latlon(meta, px, py, elev, lat, lon, &hgt);
    } else {
        proj_to_latlon(meta->projection, px, py, pz, lat, lon, &hgt);
        *lat *= R2D;
        *lon *= R2D;
    }
    return 0;
  }
  else if (meta->airsar) {
    double l = yLine, s = xSample;
    if (meta->sar) {
          l = meta->general->start_line +
                  meta->general->line_scaling * yLine;
          s = meta->general->start_sample +
                  meta->general->sample_scaling * xSample;
    }
    airsar_to_latlon(meta, s, l, elev, lat, lon);
    return 0;
  }
  else if (meta->transform) {
      /* ALOS data (not projected) -- use transform block */
      double l = yLine, s = xSample;
      if (meta->sar) {
          l = meta->general->start_line +
                  meta->general->line_scaling * yLine;
          s = meta->general->start_sample +
                  meta->general->sample_scaling * xSample;
      }
      alos_to_latlon(meta, s, l, elev, lat, lon, &hgt);
      return 0;
  }
  else if (meta->sar &&
           (meta->sar->image_type=='S' || meta->sar->image_type=='G')) {
      /*Slant or ground range.  Use state vectors and doppler.*/
      double slant,doppler,time;
      meta_get_timeSlantDop(meta,yLine,xSample,&time,&slant,&doppler);
      return meta_timeSlantDop2latLon(meta,
              time,slant,doppler,elev,
              lat,lon);
  } else { /*Bogus image type.*/
    asfPrintError(
      "meta_get_latLon: Couldn't figure out what kind of image this is!\n"
      "meta->transform = %p, so it isn't ALOS.\n"
      "meta->sar = %p, and it isn't Slant/Ground range.\n"
      "meta->projection = %p, so it isn't Projected, or Scansar.\n"
      "meta->general->name: %s\n",
      meta->transform, meta->sar, meta->projection,
      meta->general ? meta->general->basename : "(null)");
    return 1; /* Not Reached */
  }
  return 0;
}


/*******************************************************************
 * meta_timeSlantDop2latLon:
 * Converts the given time, slant range, doppler, and elevation off
 * earth's surface into a latitude and longitude.*/
int meta_timeSlantDop2latLon(meta_parameters *meta,
                             double time, double slant,double dop,double elev,
                             double *lat,double *lon)
{
  double ignored;
  stateVector stVec;
  stVec=meta_get_stVec(meta,time);
  fixed2gei(&stVec,0.0);/*Subtract Earth's spin.*/

  return getLatLongMeta(stVec,meta,slant,dop,elev,
                              lat,lon,&ignored);
}

/*******************************************************************
 * meta_get_timeSlantDop:
 * Converts a given line and sample in image into time, slant-range,
 * and doppler.  Works with all image types.*/
void meta_get_timeSlantDop(meta_parameters *meta,
  double yLine,double xSample,double *time,double *slant,double *dop)
{
  // No effort has been made to make this routine work with
  // pseudoprojected images.
  assert (meta->projection == NULL
    || meta->projection->type != LAT_LONG_PSEUDO_PROJECTION);

  if (meta->sar->image_type=='S'||meta->sar->image_type=='G')
  { /*Slant or ground range.  These are easy.*/
    *slant = meta_get_slant(meta,yLine,xSample);
    *time  = meta_get_time(meta,yLine,xSample);
    if (dop != NULL)
    {
      if (meta->sar->deskewed == 1)
        *dop=0.0;
      else
        *dop=meta_get_dop(meta,yLine,xSample);
    }
  } else if (meta->sar->image_type=='P' ||
           meta->sar->image_type=='R')
  {
    double lat,lon;
    meta_get_latLon(meta,yLine,xSample,0.0,&lat,&lon);
    latLon2timeSlant(meta,lat,lon,time,slant,dop);
  } else
  {  /*Bogus image type.*/
    printf("Error! Invalid image type '%c' passed to meta_get_timeSlantDop!\n",
      meta->sar->image_type);
    exit(1);
  }
}

/******************************************************************
 * Internal utilities for meta_get_lineSamp() */
typedef struct {
  double lat;
  double lon;
} lat_lon;

static double get_distance(lat_lon one,lat_lon two)
{
  double dlat=one.lat-two.lat;
  double dlon;

  // some kludgery to handle crossing the meridian
  // get "two" to be on the same side as "one"
  if (fabs(one.lon-two.lon) > 300) {
    if (one.lon < 0 && two.lon > 0) two.lon -= 360;
    if (one.lon > 0 && two.lon < 0) two.lon += 360;
  }

  dlon = one.lon - two.lon;

  /* Scale longitude difference to take into accound the fact
     that longitude lines are a lot closer at the pole.  */
  dlon *= cos (one.lat * PI / 180.0);

  return dlat * dlat + dlon * dlon;
}

static int get_error(meta_parameters *meta,
                     lat_lon target,double elev,
                     double xSamp,double yLine,
                     double *error)
{
  lat_lon new;
  int err;
  err = meta_get_latLon(meta,yLine,xSamp,elev,&new.lat,&new.lon);
  if (err)
    return err;
  *error = get_distance(new,target);
  return 0;
}

/******************************************************************
 * meta_get_lineSamp:
 * Converts given latitude and longitude back to the original line
 * and sample.*/
static int meta_get_lineSamp_imp(meta_parameters *meta,
          double x_start, double y_start,
          double lat,double lon,double elev,
          double *yLine,double *xSamp, double tolerance)
{
  /*Number of pixels along which to
    perform finite difference approximation of the derivative.*/
  const double DELTA = 0.1;

  double x = x_start;
  double y = y_start;
  double x_old=1000, y_old=1000;
  double dx, dy;
  int iter=0,err=0;
  lat_lon target;

  target.lat = lat;
  target.lon = lon;
  while (fabs(x-x_old)+fabs(y-y_old)>DELTA*tolerance)
  {
    double cur_err, tmp, del_x, del_y, rad;

    err = get_error(meta,target,elev,x,y,&cur_err);
//printf("%lf, err=%d\n", cur_err, err);
    if (err)
      return err;

    get_error(meta,target,elev,x+DELTA,y,&tmp);
    del_x = (tmp-cur_err)/DELTA;

    get_error(meta,target,elev,x,y+DELTA,&tmp);
    del_y = (tmp-cur_err)/DELTA;

    rad = fabs(del_x) + fabs(del_y);

//printf("iter %d: x=%6.1f; y=%6.1f, cur_err=%.6f\n",iter,x,y,cur_err);

    x_old = x;
    y_old = y;
    dx = (fabs(del_x)/rad)*cur_err/del_x;
    x -= dx;
    dy = (fabs(del_y)/rad)*cur_err/del_y;
    y -= dy;

    iter++;
    if (iter>1000)
      return 1;
  }
  //printf("  %d iterations\n",iter);

  *yLine=y-DELTA/2;
  *xSamp=x-DELTA/2;

  return 0;
}

int meta_get_lineSamp(meta_parameters *meta,
                      double lat,double lon,double elev,
                      double *yLine,double *xSamp)
{
  if (meta->projection && meta->projection->type != SCANSAR_PROJECTION) {
    meta_projection *mp = meta->projection;
    meta_general *mg = meta->general;

    double x, y, z;
    latlon_to_proj(mp, 'R', lat*D2R, lon*D2R, elev, &x, &y, &z);

    *yLine = (y - mp->startY)/mp->perY - mg->start_line;
    *xSamp = (x - mp->startX)/mp->perX - mg->start_sample;

    return 0;
  }
  else {
    double *a = get_a_coeffs(meta); // Usually meta->transform->map2ls_a;
    double *b = get_b_coeffs(meta); // Usually meta->transform->map2ls_b;

    if (meta->transform && a != NULL && b  != NULL)
    {
      int pc = meta->transform->parameter_count;
      assert(pc==25 || pc==10 || pc==4);

      if (meta->transform->parameter_count == 25) {
        //printf("before - lat: %.4lf, lon: %.4lf\n", lat, lon);
        lat -= meta->transform->origin_lat;
        lon -= meta->transform->origin_lon;
        //printf("after - lat: %.4lf, lon: %.4lf\n", lat, lon);
      }
      // If a valid transform block was found, use it...
      double lat2 = lat  * lat;
      double lon2 = lon  * lon;
      double lat3 = lat2 * lat;
      double lon3 = lon2 * lon;
      double lat4 = lat2 * lat2;
      double lon4 = lon2 * lon2;

      if (meta->transform->parameter_count == 25) {
        *xSamp = a[0] *lat4*lon4 + a[1] *lat4*lon3 + a[2] *lat4*lon2 +
                 a[3] *lat4*lon  + a[4] *lat4      + a[5] *lat3*lon4 + a[6] *lat3*lon3 +
                 a[7] *lat3*lon2 + a[8] *lat3*lon  + a[9] *lat3      + a[10]*lat2*lon4 +
                 a[11]*lat2*lon3 + a[12]*lat2*lon2 + a[13]*lat2*lon  +
                 a[14]*lat2      + a[15]*lat*lon4  + a[16]*lat*lon3  + a[17]*lat*lon2  +
                 a[18]*lat*lon   + a[19]*lat       + a[20]*lon4      + a[21]*lon3      +
                 a[22]*lon2      + a[23]*lon       + a[24];
        *yLine = b[0] *lat4*lon4 + b[1] *lat4*lon3 + b[2] *lat4*lon2 +
                 b[3] *lat4*lon  + b[4] *lat4      + b[5] *lat3*lon4 + b[6] *lat3*lon3 +
                 b[7] *lat3*lon2 + b[8] *lat3*lon  + b[9] *lat3      + b[10]*lat2*lon4 +
                 b[11]*lat2*lon3 + b[12]*lat2*lon2 + b[13]*lat2*lon  +
                 b[14]*lat2      + b[15]*lat*lon4  + b[16]*lat*lon3  + b[17]*lat*lon2  +
                 b[18]*lat*lon   + b[19]*lat       + b[20]*lon4      + b[21]*lon3      +
                 b[22]*lon2      + b[23]*lon       + b[24];
      }
      else { // if (meta->transform->parameter_count == 10) {
        *xSamp = a[0]          + a[1]*lat      + a[2]*lon      + a[3]*lat*lon + a[4]*lat2 +
                 a[5]*lon2     + a[6]*lat2*lon + a[7]*lat*lon2 +
                 a[8]*lat3     + a[9]*lon3;
        *yLine = b[0]          + b[1]*lat      + b[2]*lon      + b[3]*lat*lon + b[4]*lat2 +
                 b[5]*lon2     + b[6]*lat2*lon + b[7]*lat*lon2 +
                 b[8]*lat3     + b[9]*lon3;
      }

      if (elev != 0.0) {
        // note that we don't need to worry about an expensive meta_incid()
        // call, since for Palsar it is calculated from the transform block
        double incid = meta_incid(meta, *yLine, *xSamp);

        // shift LEFT in ascending images, RIGHT in descending
        if (meta->general->orbit_direction=='A')
          *xSamp -= elev*tan(PI/2-incid)/meta->general->x_pixel_size;
        else
          *xSamp += elev*tan(PI/2-incid)/meta->general->x_pixel_size;
      }

      // we use 0-based indexing, whereas these functions are 1-based.
      if (meta->transform->parameter_count != 25) {
        *xSamp -= 1;
        *yLine -= 1;
      }

      // result is in the original line/sample count units,
      // adjust in case we have scaled
      if (meta->sar && (
            (meta->sar->original_line_count != meta->general->line_count) ||
            (meta->sar->original_sample_count != meta->general->sample_count)))
      {
        *yLine *= (double)meta->general->line_count/
                  (double)meta->sar->original_line_count;
        *xSamp *= (double)meta->general->sample_count/
                  (double)meta->sar->original_sample_count;
      }

      return 0;
    }
    else {
      double x0, y0, tol = 0.2;
      int err,num_iter = 0;

      // Plan: added a tolerance factor to meta_get_lineSamp_imp, slowly
      // increase it as we get more and more desperate for convergence.
      // We shouldn't ever get past the first iteration, the only cases
      // where this was needed were cases near the zero doppler line, which
      // was fixed up in another way.
      while (++num_iter <= 6)
      {
        //if (num_iter > 1)
        //  printf("Iteration #%d: tolerance: %f\n", num_iter, tol);

        x0 = meta->general->sample_count/2;
        y0 = meta->general->line_count/2;
        err = meta_get_lineSamp_imp(meta, x0, y0, lat, lon, elev,
            yLine, xSamp, tol);
        if (!err) return 0;

        // First attempt failed to converge... try another starting location,
        // near one of the corners.  If this corner fails, then we'll try each
        // of the other corners.
        //printf("Failed to converge at center point... trying UL corner.\n");
        x0 = meta->general->sample_count/8;
        y0 = meta->general->line_count/8;
        err = meta_get_lineSamp_imp(meta, x0, y0, lat, lon, elev,
            yLine, xSamp, tol);
        if (!err) return 0;

        //printf("Failed to converge at UL corner... trying LR corner.\n");
        x0 = 7*meta->general->sample_count/8;
        y0 = 7*meta->general->line_count/8;
        err = meta_get_lineSamp_imp(meta, x0, y0, lat, lon, elev,
            yLine, xSamp, tol);
        if (!err) return 0;

        //printf("Failed to converge at LR corner... trying LL corner.\n");
        x0 = meta->general->sample_count/8;
        y0 = 7*meta->general->line_count/8;
        err = meta_get_lineSamp_imp(meta, x0, y0, lat, lon, elev,
            yLine, xSamp, tol);
        if (!err) return 0;

        //printf("Failed to converge at LL corner... trying UR corner.\n");
        x0 = 7*meta->general->sample_count/8;
        y0 = meta->general->line_count/8;
        err = meta_get_lineSamp_imp(meta, x0, y0, lat, lon, elev,
            yLine, xSamp, tol);
        if (!err) return 0;

        //printf("Failed to converge at UR corner... trying (0,0) ??\n");
        x0 = y0 = 0.0;
        err = meta_get_lineSamp_imp(meta, x0, y0, lat, lon, elev,
            yLine, xSamp, tol);
        if (!err) return 0;

        tol += 0.2;
      }

      // Return center point just for something to return...
      *xSamp = meta->general->sample_count/2;
      *yLine = meta->general->line_count/2;
      return 1;
    }
  }
}

void meta_get_corner_coords(meta_parameters *meta)
{
  double lat, lon;

  if (!meta->location)
    meta->location = meta_location_init();
  meta_get_latLon(meta, 0, 0, 0.0, &lat, &lon);
  meta->location->lon_start_near_range = lon;
  meta->location->lat_start_near_range = lat;
  meta_get_latLon(meta, 0, meta->general->sample_count, 0.0, &lat, &lon);
  meta->location->lon_start_far_range = lon;
  meta->location->lat_start_far_range = lat;
  meta_get_latLon(meta, meta->general->line_count, 0, 0.0, &lat, &lon);
  meta->location->lon_end_near_range = lon;
  meta->location->lat_end_near_range = lat;
  meta_get_latLon(meta, meta->general->line_count, meta->general->sample_count,
                  0.0, &lat, &lon);
  meta->location->lon_end_far_range = lon;
  meta->location->lat_end_far_range = lat;
}

double *get_a_coeffs(meta_parameters *meta)
{
    // Return a valid set of coefficients for transforming
    // from lat/lon to sample (use together with get_b_coeffs()
    // for going from lat/lon to line so you will have a line/samp
    // pair.)
    double *ret = NULL;

    if (meta && meta->transform) {
        if (is_valid_ll2s_transform(meta) &&
	    meta->transform->parameter_count == 25)
	    ret = meta->transform->l;
	else if (is_valid_map2s_transform(meta)) {
            ret = meta->transform->map2ls_a;
        }
        else if (is_valid_ll2s_transform(meta)) {
            ret = meta->transform->s;
        }
        else {
            ret = NULL;
        }
    }

    return ret;
}

double *get_b_coeffs(meta_parameters *meta)
{
    // Return a valid set of coefficients for transforming
    // from lat/lon to line (use together with get_a_coeffs()
    // for going from lat/lon to sample so you will have a line/samp
    // pair.)
    double *ret = NULL;

    if (meta && meta->transform) {
        if (is_valid_ll2l_transform(meta) &&
	    meta->transform->parameter_count == 25)
	    ret = meta->transform->s;
        else if (is_valid_map2l_transform(meta)) {
            ret = meta->transform->map2ls_b;
        }
        else if (is_valid_ll2l_transform(meta)) {
            ret = meta->transform->l;
        }
        else {
            ret = NULL;
        }
    }

    return ret;
}

int is_valid_map2s_transform(meta_parameters *meta)
{
    int ret = 1, i;

    if (meta && meta->transform && meta->transform->parameter_count > 0)
    {
        for (i=0; i<meta->transform->parameter_count; i++) {
            if (!meta_is_valid_double(meta->transform->map2ls_a[i])) {
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

int is_valid_map2l_transform(meta_parameters *meta)
{
    int ret = 1, i;

    if (meta && meta->transform && meta->transform->parameter_count > 0)
    {
        for (i=0; i<meta->transform->parameter_count; i++) {
            if (!meta_is_valid_double(meta->transform->map2ls_b[i])) {
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

int is_valid_ll2l_transform(meta_parameters *meta)
{
    int ret = 1, i;

    if (meta && meta->transform && meta->transform->parameter_count > 0)
    {
        for (i=0; i<meta->transform->parameter_count; i++) {
            if (!meta_is_valid_double(meta->transform->l[i])) {
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

int is_valid_ll2s_transform(meta_parameters *meta)
{
    int ret = 1, i;

    if (meta && meta->transform && meta->transform->parameter_count > 0)
    {
        for (i=0; i<meta->transform->parameter_count; i++) {
            if (!meta_is_valid_double(meta->transform->s[i])) {
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

void meta_get_bounding_box(meta_parameters *meta,
                           double *plat_min, double *plat_max,
                           double *plon_min, double *plon_max)
{
    int nl = meta->general->line_count;
    int ns = meta->general->sample_count;

    double lat, lon;
    double lat_min, lat_max, lon_min, lon_max;
    meta_get_latLon(meta, 0, 0, 0, &lat, &lon);
    lat_min = lat;
    lat_max = lat;
    lon_min = lon;
    lon_max = lon;

    meta_get_latLon(meta, nl-1, 0, 0, &lat, &lon);
    if (lat<lat_min) lat_min=lat;
    if (lat>lat_max) lat_max=lat;
    if (lon<lon_min) lon_min=lon;
    if (lon>lon_max) lon_max=lon;

    meta_get_latLon(meta, nl-1, ns-1, 0, &lat, &lon);
    if (lat<lat_min) lat_min=lat;
    if (lat>lat_max) lat_max=lat;
    if (lon<lon_min) lon_min=lon;
    if (lon>lon_max) lon_max=lon;

    meta_get_latLon(meta, 0, ns-1, 0, &lat, &lon);
    if (lat<lat_min) lat_min=lat;
    if (lat>lat_max) lat_max=lat;
    if (lon<lon_min) lon_min=lon;
    if (lon>lon_max) lon_max=lon;

    //printf("Bounding Box:\n");
    //printf("LAT: %.1f %.1f\n", lat_min, lat_max);
    //printf("LON: %.1f %.1f\n", lon_min, lon_max);

    *plat_min = lat_min;
    *plat_max = lat_max;

    *plon_min = lon_min;
    *plon_max = lon_max;
}
