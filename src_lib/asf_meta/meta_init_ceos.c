/*******************************************************************************
FUNCTION NAME:  meta_init_ceos

DESCRIPTION:
   Extract relevant parameters from CEOS.
   Internal-only routine.

RETURN VALUE:

SPECIAL CONSIDERATIONS:

PROGRAM HISTORY:
  1.0  - O. Lawlor.           9/98   CEOS Independence.
  1.5  - P. Denny.            8/02   Formatted for new meta structure
  2.0  - P. Denny / R. Gens   9/03   ASF facility Data Record independence
                                      And merged this with meta_init_asf.c
*******************************************************************************/
#include "asf.h"
#include "asf_nan.h"
#include <ctype.h>
#include "meta_init.h"
#include "asf_endian.h"
#include "dateUtil.h"
#include "get_ceos_names.h"

/* Internal Prototypes */
void ceos_init_proj(meta_parameters *meta,  struct dataset_sum_rec *dssr,
                    struct VMPDREC *mpdr);
ceos_description *get_ceos_description(char *fName);
double get_firstTime(char *fName);

/* Prototypes from meta_init_stVec.c */
void ceos_init_stVec(char *fName,ceos_description *ceos,meta_parameters *sar);
double get_timeDelta(ceos_description *ceos,struct pos_data_rec *ppdr,
                     meta_parameters *meta);

/* Prototypes from jpl_proj.c */
void atct_init(meta_projection *proj,stateVector st);
int UTM_zone(double lon);

/* Prototype from frame_calc.c */
int asf_frame_calc(char *sensor, float latitude, char orbit_direction);

/*******************************************************************************
 * ceos_init:
 * Reads SAR structure parameters from CEOS into existing meta_parameters
 * structure.  Calls the facility-specific decoders below. */
void ceos_init(const char *in_fName,meta_parameters *meta)
{
   char dataName[255],leaderName[255];/* CEOS names, typically .D and .L      */
   char fac[50],sys[50],ver[50];     /* Fields describing the SAR processor   */
   ceos_description *ceos=NULL;
   struct dataset_sum_rec *dssr=NULL;/* Data set summary record               */
   struct IOF_VFDR *iof=NULL;        /* Image File Descriptor Record          */
   struct VMPDREC *mpdr=NULL;        /* Map Projection Data Record            */
   struct FDR *fdr=NULL;             /* File Descriptor Record                */
   struct pos_data_rec *ppdr=NULL;   /* Platform position Data Record         */
   struct PPREC *ppr=NULL;           /* Processing Parameter Record           */
   struct VFDRECV *asf_facdr=NULL;   /* ASF facility data record              */
   struct ESA_FACDR *esa_facdr=NULL; /* ESA facility data record              */
   int dataSize;
   ymd_date date;
   hms_time time;
   double firstTime, centerTime;

   /* Allocate & fetch CEOS records. If its not there, free & nullify pointer
      ----------------------------------------------------------------------*/
   require_ceos_pair(in_fName, dataName, leaderName);
   ceos = get_ceos_description(leaderName);
   dssr = &ceos->dssr;
   iof = (struct IOF_VFDR*) MALLOC(sizeof(struct IOF_VFDR));
   if ( -1 == get_ifiledr(dataName, iof))  { FREE(iof); iof = NULL; }
   mpdr = (struct VMPDREC*) MALLOC(sizeof(struct VMPDREC));
   /* Something funny about CDPF SLCs and map projection record: cheezy fix */
   if (strncmp(dssr->fac_id,"CDPF",4)!=0) {
     if ( -1 == get_mpdr(leaderName, mpdr))    { FREE(mpdr); mpdr = NULL; }
   }
   else mpdr = NULL;
   fdr = (struct FDR*) MALLOC(sizeof(struct FDR));
   if ( -1 == get_fdr(leaderName, fdr))      { FREE(fdr); fdr = NULL; }
   ppdr = (struct pos_data_rec*) MALLOC(sizeof(struct pos_data_rec));
   if ( -1 == get_ppdr(leaderName, ppdr))    {FREE(ppdr); ppdr = NULL; }
   ppr = (struct PPREC*) MALLOC(sizeof(struct PPREC));
   if ( -1 == get_ppr(leaderName, ppr))      { FREE(ppr); ppr = NULL; }
   /* Fill either asf_facdr or esa_facdr depending on which is there */
   if ((fdr!=NULL) && (fdr->l_facdr==1717)) {
      asf_facdr=(struct VFDRECV*)MALLOC(sizeof(struct VFDRECV));
      if ( -1 == get_asf_facdr(leaderName, asf_facdr))
         { FREE(asf_facdr); asf_facdr = NULL; }
   }
   else if ((fdr->l_facdr==12288) && (strncmp(dssr->fac_id,"CDPF",4)!=0)) {
      esa_facdr=(struct ESA_FACDR*)MALLOC(sizeof(struct ESA_FACDR));
      if ( -1 == get_esa_facdr(leaderName, esa_facdr))
         { FREE(esa_facdr); esa_facdr = NULL; }
   }

 /* Fill meta->general structure */
   /* Determine satellite and beam mode */
   strcpy(meta->general->sensor, dssr->mission_id);
   strtok(meta->general->sensor," ");/*Remove spaces from field.*/
   if (strlen(dssr->beam1) <= (MODE_FIELD_STRING_MAX)) {
      strcpy(meta->general->mode, dssr->beam1);
   }
   if ((strncmp(dssr->sensor_id,"ERS-1",5)==0) || (strncmp(dssr->mission_id,"ERS1",4)==0)) {
      strcpy(meta->general->sensor,"ERS1");
      strcpy(meta->general->mode, "STD");
   }
   else if ((strncmp(dssr->sensor_id,"ERS-2",5)==0) || (strncmp(dssr->mission_id,"ERS2",4)==0)) {
      strcpy(meta->general->sensor,"ERS2");
      strcpy(meta->general->mode, "STD");
   }
   else if (strncmp(dssr->sensor_id,"JERS-1",6)==0) {
      strcpy(meta->general->sensor,"JERS1");
      strcpy(meta->general->mode, "STD");
   }
   else if (strncmp(dssr->sensor_id,"RSAT-1",6)==0) {
/* probably need to check incidence angle to figure out what is going on */
      char beamname[32];
      int ii;
      for (ii=0; ii<32; ii++) { beamname[ii] = '\0'; }
      strcpy(meta->general->sensor,"RSAT-1");
      if (strncmp(dssr->product_type,"SCANSAR",7)==0) {
        if (strncmp(dssr->beam3,"WD3",3)==0) strcpy(beamname,"SWA");
        else if (strncmp(dssr->beam3,"ST5",3)==0) strcpy(beamname,"SWB");
        else if (strncmp(dssr->beam3,"ST6",3)==0) strcpy(beamname,"SNA");
        else strcpy(beamname,"SNB");
      }
      else {
        int beamnum = atoi(&(dssr->beam1[2]));
        switch(dssr->beam1[0]) {
          case 'S': sprintf(beamname,"ST%i",beamnum); break;
          case 'W': sprintf(beamname,"WD%i",beamnum); break;
          case 'F': sprintf(beamname,"FN%i",beamnum); break;
          case 'E':
            if (dssr->beam1[1]=='H') sprintf(beamname,"EH%i",beamnum);
            else sprintf(beamname,"EL%i",beamnum);
	    break;
        }
      }
      if ((ppr) &&
          (   (strncmp(dssr->fac_id,"CDPF",4)==0)
           || (strncmp(dssr->fac_id,"FOCUS",5)==0))) {
        strcpy(beamname, ppr->beam_type);
      }
      strcpy(meta->general->mode, beamname);
   }
   strcpy(fac,dssr->fac_id);strtok(fac," ");/*Remove spaces from field*/
   strcpy(sys,dssr->sys_id);strtok(sys," ");/*Remove spaces from field*/
   strcpy(ver,dssr->ver_id);strtok(ver," ");/*Remove spaces from field*/
   sprintf(meta->general->processor,"%s/%s/%s",fac,sys,ver);
   /* FOCUS data header is erroneous, hence the if statement */
   if ((iof->bitssamp*iof->sampdata)>(iof->bytgroup*8)) iof->bitssamp /= 2;
   dataSize = (iof->bitssamp+7)/8 + (iof->sampdata-1)*5;
   if ((dataSize<6) && (strncmp(iof->formatid, "COMPLEX", 7)==0))
      dataSize += (10 - dataSize)/2;
   switch (dataSize) {
      case 2:  meta->general->data_type = INTEGER16;         break;
      case 4:  meta->general->data_type = INTEGER32;         break;
      case 6:  meta->general->data_type = COMPLEX_BYTE;      break;
      case 7:  meta->general->data_type = COMPLEX_INTEGER16; break;
      default: meta->general->data_type = BYTE;              break;
   }
   strcpy(meta->general->system, meta_get_system());
   meta->general->orbit = atoi(dssr->revolution);

   meta->general->band_number      = 0;
   meta->general->orbit_direction  = dssr->asc_des[0];
   if (meta->general->orbit_direction==' ')
     meta->general->orbit_direction = (meta->general->frame>=1791 && meta->general->frame<=5391) ? 'D' : 'A';
   meta->general->line_count       = iof->numofrec;
   if (asf_facdr)
      meta->general->sample_count  = asf_facdr->npixels;
   else if (((iof->reclen-iof->predata-iof->sufdata)/iof->bytgroup) == (dssr->sc_pix*2))
      meta->general->sample_count  = (iof->reclen-iof->predata-iof->sufdata)/iof->bytgroup;
   else
      meta->general->sample_count  = iof->sardata/iof->bytgroup;
   meta->general->start_line       = 0;
   meta->general->start_sample     = 0;
   meta->general->x_pixel_size     = dssr->pixel_spacing;
   meta->general->y_pixel_size     = dssr->line_spacing;
   if (asf_facdr && ceos->product!=CCSD) {
   /* For ASF SLC, the spacings are incorrect in the ASFFACDR - Tom Logan 3/01*/
      if (asf_facdr->rapixspc > 0.0)
         meta->general->x_pixel_size = asf_facdr->rapixspc;
      if (asf_facdr->azpixspc > 0.0)
         meta->general->y_pixel_size = asf_facdr->azpixspc;
   }
   meta->general->center_latitude  = dssr->pro_lat;
   meta->general->center_longitude = dssr->pro_long;

   /* Calculate ASF frame number from latitude considering the orbit direction */
   meta->general->frame =
     asf_frame_calc(meta->general->sensor, meta->general->center_latitude, meta->general->orbit_direction);

   meta->general->re_major         = (dssr->ellip_maj < 10000.0) ? dssr->ellip_maj*1000.0 : dssr->ellip_maj;
   meta->general->re_minor         = (dssr->ellip_min < 10000.0) ? dssr->ellip_min*1000.0 : dssr->ellip_min;
   if (asf_facdr)      meta->general->bit_error_rate = asf_facdr->biterrrt;
   else if (esa_facdr) meta->general->bit_error_rate = esa_facdr->ber;
   else                meta->general->bit_error_rate = 0.0;

 /* Fill meta->sar structure */
   if (mpdr) {
      meta->sar->image_type = 'P';
   }
   else if (asf_facdr) {
     if (0==strncmp(asf_facdr->grndslnt,"GROUND",6))
       meta->sar->image_type='G';
     else
       meta->sar->image_type='S';
   }
   else {
      if (ceos->product==CCSD || ceos->product==SLC || ceos->product==RAW) {
         meta->sar->image_type = 'S';
      }
      else if (ceos->product==LOW_REZ || ceos->product==HI_REZ || ceos->product==SGF) {
         meta->sar->image_type = 'G';
      }
   }
   meta->sar->look_direction = (dssr->clock_ang>=0.0) ? 'R' : 'L';
   switch (ceos->satellite) {
      case  ERS: meta->sar->look_count = 5; break;
      case JERS: meta->sar->look_count = 3; break;
      case RSAT:
         dssr->rng_samp_rate *= get_units(dssr->rng_samp_rate,EXPECTED_RSR);
         dssr->rng_gate *= get_units(dssr->rng_gate,EXPECTED_RANGEGATE);
         if (dssr->rng_samp_rate < 20.0) /* split finebeam from the rest */
            meta->sar->look_count = 4; /* ST1-ST7, WD1-WD3, EL1, EH1-EH6 */
         else
            meta->sar->look_count = 1; /* FN1-FN5 */
         break;
   }
   if (asf_facdr) {
      if (toupper(asf_facdr->deskewf[0])=='Y')
         meta->sar->deskewed = 1;
      else
         meta->sar->deskewed = 0;
   }
   else if (esa_facdr)
      meta->sar->deskewed = 1;
   else
      meta->sar->deskewed = 0;
   if (asf_facdr) {
      meta->sar->original_line_count   = asf_facdr->nlines;
      meta->sar->original_sample_count = asf_facdr->apixels;
   }
   else {
      meta->sar->original_line_count   = iof->numofrec;
      meta->sar->original_sample_count = (iof->reclen-iof->predata-iof->sufdata)
                                         / iof->bytgroup;
      if ( meta->sar->original_line_count==0
          || meta->sar->original_sample_count==0) {
         meta->sar->original_line_count   = dssr->sc_lin*2;
         meta->sar->original_sample_count = dssr->sc_pix*2;
      }
   }
   /* FOCUS precision image data need correct number of samples */
   if (ceos->processor==FOCUS && ceos->product==PRI) {
      meta->sar->original_sample_count = iof->datgroup;
   }
   meta->sar->line_increment   = 1.0;
   meta->sar->sample_increment = 1.0;
   meta->sar->range_time_per_pixel = dssr->n_rnglok
           / (dssr->rng_samp_rate * get_units(dssr->rng_samp_rate,EXPECTED_FS));
   if (asf_facdr) {
      meta->sar->azimuth_time_per_pixel = meta->general->y_pixel_size
                                          / asf_facdr->swathvel;
   }
   else {
      firstTime = get_firstTime(dataName);
      if (esa_facdr && (ceos->facility != VEXCEL)) {
        date_dssr2time(dssr->az_time_first, &time);
        firstTime = date_hms2sec(&time);
      }
      date_dssr2date(dssr->inp_sctim, &date, &time);
      centerTime = date_hms2sec(&time);
      meta->sar->azimuth_time_per_pixel = (centerTime - firstTime) / dssr->sc_lin;
   }
   /* CEOS data does not account for slant_shift and time_shift errors so far as
    * we can tell.  Other ASF tools may later set these fields based on more
    * precise orbit data.  */
   meta->sar->slant_shift = 0.0;
   meta->sar->time_shift = 0.0;
   /* For ASP images, flip the image top-to-bottom: */
   if ( (asf_facdr)
       && (ceos->processor==ASP||ceos->processor==SPS||ceos->processor==PREC)) {
      meta->sar->time_shift += meta->sar->azimuth_time_per_pixel
                               * asf_facdr->alines;
      meta->sar->azimuth_time_per_pixel *= -1.0;
   }
   if (asf_facdr) {
      meta->sar->slant_range_first_pixel = asf_facdr->sltrngfp * 1000.0;
   }
   else if (esa_facdr) {
      meta->sar->slant_range_first_pixel = dssr->rng_time[0]*speedOfLight/2000.0;
   }
   else {
      meta->sar->slant_range_first_pixel = dssr->rng_gate
	     * get_units(dssr->rng_gate,EXPECTED_RANGEGATE) * speedOfLight / 2.0;
   }
   meta->sar->wavelength = dssr->wave_length * get_units(dssr->wave_length,EXPECTED_WAVELEN);
   meta->sar->prf = dssr->prf;
   if (asf_facdr) {     /* Get earth radius & satellite height if we can */
      meta->sar->earth_radius = asf_facdr->eradcntr*1000.0;
      meta->sar->satellite_height = meta->sar->earth_radius
                                     + asf_facdr->scalt*1000;
   }
   if (ceos->facility==CDPF) {
   /* Doppler centroid values stored in Doppler rate fields */
      meta->sar->range_doppler_coefficients[0] = dssr->crt_rate[0];
      meta->sar->range_doppler_coefficients[1] = dssr->crt_rate[1];
      meta->sar->range_doppler_coefficients[2] = dssr->crt_rate[2];
   }
   else if (ceos->facility==ESA) {
   /* D-PAF and I-PAF give Doppler centroid values in Hz/sec */
      meta->sar->range_doppler_coefficients[0] = dssr->crt_dopcen[0];
      meta->sar->range_doppler_coefficients[1] = /*two-way range time*/
                                          dssr->crt_dopcen[1] / (speedOfLight * 2);
      meta->sar->range_doppler_coefficients[2] = /*two-way range time*/
                         dssr->crt_dopcen[2] / (speedOfLight * speedOfLight * 4);
   }
   else {
      meta->sar->range_doppler_coefficients[0] = dssr->crt_dopcen[0];
      meta->sar->range_doppler_coefficients[1] = dssr->crt_dopcen[1];
      meta->sar->range_doppler_coefficients[2] = dssr->crt_dopcen[2];
   }
   meta->sar->azimuth_doppler_coefficients[0] = dssr->alt_dopcen[0];
   meta->sar->azimuth_doppler_coefficients[1] = dssr->alt_dopcen[1];
   meta->sar->azimuth_doppler_coefficients[2] = dssr->alt_dopcen[2];
   /* check Doppler number whether they make sense, otherwise set to 'NaN' */
   if (fabs(meta->sar->range_doppler_coefficients[0])>=15000) {
     meta->sar->range_doppler_coefficients[0]=MAGIC_UNSET_DOUBLE;
     meta->sar->range_doppler_coefficients[1]=MAGIC_UNSET_DOUBLE;
     meta->sar->range_doppler_coefficients[2]=MAGIC_UNSET_DOUBLE;
   }
   if (fabs(meta->sar->azimuth_doppler_coefficients[0])>=15000) {
     meta->sar->azimuth_doppler_coefficients[0]=MAGIC_UNSET_DOUBLE;
     meta->sar->azimuth_doppler_coefficients[1]=MAGIC_UNSET_DOUBLE;
     meta->sar->azimuth_doppler_coefficients[2]=MAGIC_UNSET_DOUBLE;
   }
   strcpy(meta->sar->satellite_binary_time,dssr->sat_bintim);
   strtok(meta->sar->satellite_binary_time," ");/*Remove spaces from field*/
   strcpy(meta->sar->satellite_clock_time, dssr->sat_clktim);
   strtok(meta->sar->satellite_clock_time, " ");/*Remove spaces from field*/

 /* Fill meta->state_vectors structure. Call to ceos_init_proj requires that the
  * meta->state_vectors structure be filled */
   ceos_init_stVec(leaderName,ceos,meta);

   /* UK-PAF provides only one state vector with its raw data.
      Copy the contents over to create two other ones for the propagation */
/* This functionality is not yet implemented.
 * if (ceos->facility==UK) {
 *   int ii;
 *   meta_state_vectors *s;
 *   s = meta_state_vectors_init(3);
 *   meta->state_vectors->vector_count = 3;
 *   for (ii=0; ii<3; ii++) {
 *     s->vecs[ii].vec = meta->state_vectors->vecs[0].vec;
 *     s->vecs[ii].time = meta->state_vectors->vecs[0].time;
 *   }
 *   meta->state_vectors = s;
 * }
 */

   /* Propagate state vectors if they are covering more than frame size in case
    * you have raw or complex data. */
   if (ceos->facility!=ESA) {
      int vector_count=3;
      double data_int = dssr->sc_lin * fabs(meta->sar->azimuth_time_per_pixel);
      get_timeDelta(ceos, ppdr, meta);
      if (meta->general->data_type>=COMPLEX_BYTE) {
        while (data_int > 15.0) {
          data_int /= 2;
          vector_count = vector_count*2-1;
        }
        /* propagate three state vectors: start, center, end */
        propagate_state(meta, vector_count, data_int);
      }
   }

   if (meta->sar->image_type=='P') {
      ceos_init_proj(meta, dssr, mpdr);
   }

   FREE(ceos);
/* FREE(dssr); Don't free dssr; it points to the ceos struct (ceos->dssr) */
   FREE(iof);
   FREE(mpdr);
   FREE(fdr);
   FREE(ppdr);
   FREE(ppr);
   FREE(asf_facdr);
   FREE(esa_facdr);
}



/*******************************************************************************
 * ceos_init_proj:
 * Allocate a projection parameters structure, given an ASF map projection data
 * record. */
void ceos_init_proj(meta_parameters *meta,  struct dataset_sum_rec *dssr,
                    struct VMPDREC *mpdr)
{
   meta_projection *projection = meta->projection =
           (meta_projection *)MALLOC(sizeof(meta_projection));

   meta->sar->image_type = 'P';/*Map-Projected image.*/
   meta->general->sample_count = mpdr->npixels;

   if ((strncmp(mpdr->mpdesc, "SLANT RANGE", 11) == 0) ||
      (strncmp(mpdr->mpdesc, "Slant range", 11) == 0)) {
      /* FOCUS processor populates map projection record for slant range! */
      /* ESA (I-PAF) apparently does the same */
      meta->sar->image_type='S';
      projection->type=MAGIC_UNSET_CHAR;
   }
   else if ((strncmp(mpdr->mpdesc, "GROUND RANGE", 12) == 0) ||
      (strncmp(mpdr->mpdesc, "Ground range", 12) == 0)) {
      /* ESA populates map projection record also for ground range! */
      meta->sar->image_type='G';
      projection->type=MAGIC_UNSET_CHAR;
   }
   else if (strncmp(mpdr->mpdesig, "GROUND RANGE",12) == 0) {
      projection->type=SCANSAR_PROJECTION;/*Along Track/Cross Track.*/
   }
   else if (strncmp(mpdr->mpdesig, "LAMBERT", 7) == 0) {
      projection->type=LAMBERT_CONFORMAL_CONIC;/*Lambert Conformal Conic.*/
      printf("WARNING: * Images geocoded with the Lambert Conformal Conic projection may not\n"
             "         * be accurately geocoded!\n");
      projection->param.lamcc.plat1=mpdr->nsppara1;
      projection->param.lamcc.plat2=mpdr->nsppara2;
      projection->param.lamcc.lat0=mpdr->blclat+0.023;/*NOTE: This line is a hack.*/
      projection->param.lamcc.lon0=mpdr->blclong+2.46;/*NOTE: This line is a hack, too.*/
   /* NOTE: We have to hack the lamcc projection because the true lat0 and lon0,
    * as far as we can tell, are never stored in the CEOS
    */
   }
   else if (strncmp(mpdr->mpdesig, "UPS", 3) == 0) {
      projection->type=POLAR_STEREOGRAPHIC;/*Polar Stereographic: pre-radarsat era*/
      projection->param.ps.slat=70.0;
      projection->param.ps.slon=-45.0;
   }
   else if (strncmp(mpdr->mpdesig, "PS-SMM/I", 8) == 0) {
      projection->type=POLAR_STEREOGRAPHIC;/*Polar Stereographic: radarsat era.*/
      projection->param.ps.slat=mpdr->upslat;
      projection->param.ps.slon=mpdr->upslong;
      if (projection->param.ps.slat>0 && projection->param.ps.slon==0.0)
        projection->param.ps.slon=-45.0;/*Correct reference longitude bug*/
   }
   else if (strncmp(mpdr->mpdesig, "UTM", 3) == 0) {
      projection->type=UNIVERSAL_TRANSVERSE_MERCATOR;/*Universal Transverse Mercator*/
      projection->param.utm.zone=UTM_zone(mpdr->utmpara1);
   }
   else {
      printf("Cannot match projection '%s',\n"
         "in map projection data record.\n",mpdr->mpdesig);
      exit(EXIT_FAILURE);
   }

    /* The Along-Track/Cross-Track projection requires special initialization*/
   if (projection->type==SCANSAR_PROJECTION)
   {
      stateVector st_start;

      projection->param.atct.rlocal = meta_get_earth_radius(meta,
                                      meta->general->line_count/2, 0);
      st_start=meta_get_stVec(meta,0.0);
      fixed2gei(&st_start,0.0);/*Remove earth's spin JPL's AT/CT projection requires this*/
      atct_init(meta->projection,st_start);
      projection->startY = mpdr->tlceast;
      projection->startX = mpdr->tlcnorth;
      projection->perY   = (mpdr->blceast - mpdr->tlceast)
                           / mpdr->nlines;
      projection->perX   = (mpdr->trcnorth-mpdr->tlcnorth)
                           / mpdr->npixels;
   }
   else {
      projection->startY = mpdr->tlcnorth;
      projection->startX = mpdr->tlceast;
      projection->perY   = (mpdr->blcnorth - mpdr->tlcnorth)
                           / mpdr->nlines;
      projection->perX   = (mpdr->trceast - mpdr->tlceast)
                           / mpdr->npixels;
   }

   /* Default the units to meters */
   strcpy(projection->units,"meters");

   projection->hem = (dssr->pro_lat>0.0) ? 'N' : 'S';

   projection->re_major = dssr->ellip_maj*1000;
   projection->re_minor = dssr->ellip_min*1000;
}


/*******************************************************************************
 * get_ceos_description:
 * Extract a ceos_description structure from given CEOS file. This contains
 * "meta-meta-"data, data about the CEOS, such as the generating facility, a
 * decoded product type, etc.*/
ceos_description *get_ceos_description(char *fName)
{
   char *versPtr,*satStr;
   char *prodStr,*procStr;
   ceos_description *ceos=(ceos_description *)MALLOC(sizeof(ceos_description));
/*Fetch DSSR*/
   get_dssr(fName,&ceos->dssr);

/*Determine the sensor.*/
   satStr=ceos->dssr.mission_id;
   if (0==strncmp(satStr,"E",1)) ceos->satellite=ERS;
   else if (0==strncmp(satStr,"J",1)) ceos->satellite=JERS;
   else if (0==strncmp(satStr,"R",1)) ceos->satellite=RSAT;
   else {
      printf("get_ceos_description Warning! Unknown sensor '%s'!\n",satStr);
      ceos->satellite=unknownSatellite;
   }

/*Determine the processor version.*/
   ceos->version=0.0;/*Default is zero.*/
   versPtr=ceos->dssr.ver_id;
   while (!isdigit(*versPtr)) versPtr++;
   sscanf(versPtr,"%lf",&ceos->version);

/*Set other fields to unknown (to be filled out by other init routines)*/
   procStr=ceos->dssr.sys_id;
   prodStr=ceos->dssr.product_type;
   ceos->processor=unknownProcessor;
   ceos->product=unknownProduct;

/*Determine the facility that processed the data.*/
   if (0==strncmp(ceos->dssr.fac_id,"ASF",3))
   {/*Alaska SAR Facility Image*/
/*Determine the image type and processor ID.*/
      ceos->facility=ASF;
      if (0==strncmp(procStr,"ASP",3)) ceos->processor=ASP;
      else if (0==strncmp(procStr,"SPS",3)) ceos->processor=SPS;
      else if (0==strncmp(procStr,"PREC",3)) ceos->processor=PREC;
      else if (0==strncmp(procStr,"AISP",4)) ceos->processor=AISP;
      else if (0==strncmp(procStr,"PP",2)) ceos->processor=PP;
      else if (0==strncmp(procStr,"SP2",3)) ceos->processor=SP2;
      else if (0==strncmp(procStr,"AMM",3)) ceos->processor=AMM;
      /* VEXCEL Focus processor */
      else if (0==strncmp(procStr,"FOCUS",5)) ceos->processor=FOCUS;
      else if (0==strncmp(procStr,"SKY",3))
      {/*Is VEXCEL level-0 processor, not ASF*/
         ceos->facility=VEXCEL;
         ceos->processor=LZP;
         ceos->product=CCSD;
         return ceos;
      }
      else if (0==strncmp(procStr, "PC", 2)) {
        if (0==strncmp(prodStr,"SCANSAR",7)) ceos->processor=SP3;
	else if (0==strncmp(prodStr,"FUL",3)) ceos->processor=PREC;
      }
      else {
         printf("get_ceos_description Warning! Unknown ASF processor '%s'!\n",procStr);
         ceos->processor=unknownProcessor;
      }


      if (0==strncmp(prodStr,"LOW",3)) ceos->product=LOW_REZ;
      else if (0==strncmp(prodStr,"FUL",3)) ceos->product=HI_REZ;
      else if (0==strncmp(prodStr,"SCANSAR",7)) ceos->product=SCANSAR;
      else if (0==strncmp(prodStr,"CCSD",4)) ceos->product=CCSD;
      else if (0==strncmp(prodStr,"COMPLEX",7)) ceos->product=SLC;
      /* Non-ASF data */
      else if (0==strncmp(prodStr,"SPECIAL PRODUCT(SINGL-LOOK COMP)",32))
         ceos->product=SLC;
      else if (0==strncmp(prodStr, "SLANT RANGE COMPLEX",19)) ceos->product=SLC;
      else if (0==strncmp(prodStr, "SAR PRECISION IMAGE",19)) ceos->product=PRI;
      else if (0==strncmp(prodStr, "SAR GEOREF FINE",15)) ceos->product=SGF;
      else {
         printf("get_ceos_description Warning! Unknown ASF product type '%s'!\n",prodStr);
         ceos->product=unknownProduct;
      }

   }
   else if (0==strncmp(ceos->dssr.fac_id,"ES",2))
   {/*European Space Agency Image*/
      printf("   Data set processed by ESA\n");
      ceos->facility=ESA;

      if (0==strncmp(prodStr,"SAR RAW SIGNAL",14)) ceos->product=RAW;
      if (0==strncmp(prodStr,"SAR PRECISION IMAGE",19)) ceos->product=PRI;
      else {
         printf("Get_ceos_description Warning! Unknown ESA product type '%s'!\n",prodStr);
         ceos->product=unknownProduct;
      }
   }
   else if (0==strncmp(ceos->dssr.fac_id,"CDPF",4))
   {
      printf("   Data set processed by CDPF\n");
      ceos->facility=CDPF;

      if (0==strncmp(prodStr,"SPECIAL PRODUCT(SINGL-LOOK COMP)",32)) ceos->product=SLC;
      else if (0==strncmp(prodStr,"SCANSAR WIDE",12)) ceos->product=SCANSAR;
      else {
         printf("Get_ceos_description Warning! Unknown CDPF product type '%s'!\n",prodStr);
         ceos->product=unknownProduct;
      }
   }
   else if (0==strncmp(ceos->dssr.fac_id,"D-PAF",5)) {
      printf("   Data set processed by D-PAF\n");
      ceos->facility=ESA;
      if (0==strncmp(prodStr,"SAR RAW SIGNAL",14)) ceos->product=RAW;
   }
   else if (0==strncmp(ceos->dssr.fac_id,"I-PAF",5)) {
      printf("   Data set processed by I-PAF\n");
      ceos->facility=ESA;
      if (0==strncmp(prodStr,"SAR RAW SIGNAL",14)) ceos->product=RAW;
   }
/* This functionality is not yet implemented.
 * else if (0==strncmp(ceos->dssr.fac_id,"UK-WFS",6)) {
 *    printf("   Data set processed by UK-WFS\n");
 *    ceos->facility=UK;
 * }
 */
   else {
      printf( "****************************************\n"
         "SEVERE WARNING!!!!  Unknown CEOS Facility '%s'!\n"
         "****************************************\n",
         ceos->dssr.fac_id);
      ceos->facility=unknownFacility;
   }

   return ceos;
}

/*---------------------------------
function extracts the acquisition time of the first line
out of the line header
-----------------------------------*/
double get_firstTime (char *fName)
{
   FILE *fp;
   struct HEADER hdr;
   struct RHEADER linehdr;
   int length;
   char buff[25600];

   fp = FOPEN(fName, "r");
   FREAD (&hdr, sizeof(struct HEADER), 1, fp);
   FREAD (&linehdr, sizeof(struct RHEADER), 1, fp);
   length = bigInt32(hdr.recsiz) - (sizeof(struct RHEADER)
            + sizeof(struct HEADER));
   FREAD (buff, length, 1, fp);
   FREAD (&hdr, sizeof(struct HEADER), 1, fp);
   FREAD (&linehdr, sizeof(struct RHEADER), 1, fp);
   FCLOSE(fp);

   return (double)bigInt32((unsigned char *)&(linehdr.acq_msec))/1000.0;
}
