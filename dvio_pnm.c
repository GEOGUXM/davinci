#include <stdio.h>
#include <stdlib.h>
#include "parser.h"
#include "dvio.h"

Var *
dv_LoadPNM(FILE *fp, char *filename, struct iom_iheader *s)
{
	u_char *data;
	struct iom_iheader h;
	int status;
	char hbuf[HBUFSIZE];
	Var *v;

	if (iom_isPNM(fp) == 0){ return NULL; }

	if (!(status = iom_GetPNMHeader(fp, filename, &h))){
		return NULL;
	}

	if (s != NULL) {
		/**
		 ** Set subsets
		 **/
		iom_MergeHeaderAndSlice(&h, s);
	}

	data = iom_read_qube_data(fileno(fp), &h);
	if (data) {
		v = iom_iheader2var(&h);
		V_DATA(v) = data;
	} else {
		v = NULL;
	}

	sprintf(hbuf, "%s: PNM %s image: %dx%dx%d, %d bits",
			filename, iom_Org2Str(h.org),
			iom_GetSamples(h.dim, h.org),
			iom_GetLines(h.dim, h.org),
			iom_GetBands(h.dim, h.org),
			iom_NBYTESI(h.format)*8);
	if (VERBOSE > 1) {
		parse_error(hbuf);
	}
	
	iom_cleanup_iheader(&h);
	return v;
}

int
dv_WritePGM(Var *obj, char *filename, int force)
{
	struct iom_iheader h;
	int status;

	if (V_TYPE(obj) != ID_VAL || V_FORMAT(obj) != DV_UINT8) {
		sprintf(error_buf, "Data for PGM file must be byte()");
		parse_error(NULL);
		return 0;
	}

	if (GetBands(V_SIZE(obj), V_ORG(obj)) != 1) {
		sprintf(error_buf, "Data for PGM file must have depth=1");
		parse_error(NULL);
		return 0;
	}

	var2iom_iheader(obj, &h);

	if (VERBOSE > 1)  {
		fprintf(stderr, "Writing %s: %dx%d PGM file.\n",
				filename, iom_GetSamples(h.size,h.org),
				iom_GetLines(h.size,h.org));
	}

	status = iom_WritePNM(filename, V_DATA(obj), &h, force);
	iom_cleanup_iheader(&h);

	if (status == 0){
		parse_error("Writing of PGM file %s failed.\n", filename);
		return 0;
	}

	return 1;
}

int
dv_WritePPM(Var *obj, char *filename, int force)
{
	struct iom_iheader h;
	int status;

	if (V_TYPE(obj) != ID_VAL || V_FORMAT(obj) != DV_UINT8) {
		sprintf(error_buf, "Data for PPM output must be byte()");
		parse_error(NULL);
		return 0;
	}

	if (GetBands(V_SIZE(obj), V_ORG(obj)) != 3) {
		sprintf(error_buf, "Data for PPM output must have depth=3");
		parse_error(NULL);
		return 0;
	}

	if (V_ORG(obj) != BIP) {
		sprintf(error_buf, "Data for PPM output must be in BIP format");
		parse_error(NULL);
		return 0;
	}

	var2iom_iheader(obj, &h);

	if (VERBOSE > 1)  {
		fprintf(stderr, "Writing %s: %dx%dx%d PPM file.\n", filename,
		        iom_GetSamples(h.size,h.org),
		        iom_GetLines(h.size,h.org),
		        iom_GetBands(h.size,h.org));
	}

	status = iom_WritePNM(filename, V_DATA(obj), &h, force);
	iom_cleanup_iheader(&h);

	if (status == 0){
		parse_error("Writing of PPM file %s failed.\n", filename);
		return 0;
	}

	return 1;
}
