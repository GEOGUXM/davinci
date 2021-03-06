#include "func.h"
#include "parser.h"

/**
 ** histogram()
 **
 ** rgb2hsi()
 ** hsi2rgb()
 ** threshold()
 ** saturate()
 ** scale()
 ** stretch()
 ** color_index()
 **/

/**
 ** compute histogram of an image
 **/

typedef struct {
	float r, g, b;
} RGB;
typedef struct {
	float h, s, v;
} HSV;
typedef struct {
	float c, m, y;
} CMY;
typedef struct {
	float c, m, y, k;
} KCMY;
RGB HSVToRGB(HSV hsv);
HSV RGBToHSV(RGB rgb);

Var* fb_min(Var* obj, int axis, int direction);

Var* ff_histogram(vfuncptr func, Var* arg)
{
	Var *obj = NULL, *compress = NULL, *normalize = NULL, *cumulative = NULL;
	int x, y, z, i, j, dsize;
	float* data;
	float v;

	float start = FLT_MAX, size = FLT_MAX;
	int steps = INT_MAX;

	Alist alist[9];
	alist[0]      = make_alist("object", ID_VAL, NULL, &obj);
	alist[1]      = make_alist("compress", ID_VAL, NULL, &compress);
	alist[2]      = make_alist("normalize", ID_VAL, NULL, &normalize);
	alist[3]      = make_alist("cumulative", ID_VAL, NULL, &cumulative);
	alist[4]      = make_alist("start", DV_FLOAT, NULL, &start);
	alist[5]      = make_alist("size", DV_FLOAT, NULL, &size);
	alist[6]      = make_alist("steps", DV_INT32, NULL, &steps);
	alist[7].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (obj == NULL) {
		parse_error("%s: No object specified\n", func->name);
		return (NULL);
	}

	x     = GetSamples(V_SIZE(obj), V_ORG(obj));
	y     = GetLines(V_SIZE(obj), V_ORG(obj));
	z     = GetBands(V_SIZE(obj), V_ORG(obj));
	dsize = V_DSIZE(obj);

	switch (V_FORMAT(obj)) {
	case DV_UINT8:
		if (start == FLT_MAX) start = 0;
		if (size == FLT_MAX) size   = 1;
		if (steps == INT_MAX) steps = 256;
		break;
	case DV_INT16:
		if (start == FLT_MAX) start = -32768;
		if (size == FLT_MAX) size   = 1;
		if (steps == INT_MAX) steps = 65536;
		break;
	case DV_INT32:
		if (steps == FLT_MAX) {
			parse_error("%s(...steps=...) required for DV_INT32 format.", func->name);
			return (NULL);
		}
		break;
	case DV_FLOAT:
		if (steps == FLT_MAX) {
			parse_error("%s(...steps=...) required for DV_FLOAT format.", func->name);
			return (NULL);
		}
		break;
	case DV_DOUBLE:
		if (steps == FLT_MAX) {
			parse_error("%s(...steps=...) required for DV_DOUBLE format.", func->name);
			return (NULL);
		}
		break;
	}

	/*
	** Find minimum if necessary
	*/
	if (start == FLT_MAX) {
		Var* vmin;
		vmin            = fb_min(obj, 7, 0);
		if (vmin) start = V_FLOAT(vmin);
	}

	/*
	** find maximum if necessary
	*/
	if (size == FLT_MAX) {
		Var* vmax;
		vmax           = fb_min(obj, 7, 1);
		if (vmax) size = (V_FLOAT(vmax) - start) / steps;
	}

	if (start == FLT_MAX || steps == INT_MAX || steps == 0 || size == FLT_MAX) {
		parse_error("Unable to determine start, steps or size");
		return (NULL);
	}

	data = (float*)calloc((steps + 1) * 2, sizeof(float));

	/*
	** X axis
	*/
	for (i = 0; i < steps; i++) {
		data[i * 2] = start + i * size;
	}

	for (i = 0; i < dsize; i++) {
		v = extract_float(obj, i);
		j = (v - start) / size;
		if (j < 0) {
			j = 0;
		} else if (j >= steps) {
			j = steps - 1;
		}
		data[j * 2 + 1]++;
	}

	/*
	** Exercise the options to compress, accumulate or normalize
	*/

	j = steps;
	if (compress != NULL) {
		j = 0;
		for (i = j = 0; i < steps; i++) {
			if (data[i * 2 + 1] != 0) {
				data[j * 2]     = data[i * 2];
				data[j * 2 + 1] = data[i * 2 + 1];
				j++;
			}
		}
	}

	if (normalize) {
		for (i = 0; i < j; i++) {
			data[i * 2 + 1] = (float)data[i * 2 + 1] / (float)dsize;
		}
	}

	if (cumulative != NULL) {
		for (i = 1; i < j; i++) {
			data[i * 2 + 1] += data[(i - 1) * 2 + 1];
		}
	}

	return (newVal(BSQ, 2, j, 1, DV_FLOAT, data));
}

Var* ff_hstats(vfuncptr func, Var* arg)
{
	Var* obj = NULL;
	int x, y, z, i, j;
	Var *both, *avg, *stddev;
	double sum, sum2;
	double x1, y1;
	int n;

	Alist alist[2];
	alist[0]      = make_alist("object", ID_VAL, NULL, &obj);
	alist[1].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (obj == NULL) {
		parse_error("%s: No object specified\n", func->name);
		return (NULL);
	}

	x = GetSamples(V_SIZE(obj), V_ORG(obj));
	y = GetLines(V_SIZE(obj), V_ORG(obj));
	z = GetBands(V_SIZE(obj), V_ORG(obj));

	if (x != 2 || z != 1 || y < 2) {
		parse_error("Object does not look like a histogram.");
		return (NULL);
	}

	avg    = newVal(BSQ, 1, 1, 1, DV_DOUBLE, calloc(1, sizeof(double)));
	stddev = newVal(BSQ, 1, 1, 1, DV_DOUBLE, calloc(1, sizeof(double)));

	/*
	** Use one pass method
	*/
	sum  = 0;
	sum2 = 0;
	n    = 0;
	for (i = 0; i < y; i++) {
		j = cpos(0, i, 0, obj);

		x1 = extract_double(obj, j);
		y1 = extract_double(obj, j + 1);

		sum += x1 * y1;
		sum2 += (x1 * x1) * y1;
		n += y1;
	}
	V_DOUBLE(stddev) = sqrt((sum2 - (sum * sum / n)) / (n - 1));
	V_DOUBLE(avg)    = sum / n;

	both = new_struct(0);
	add_struct(both, "avg", avg);
	add_struct(both, "stddev", stddev);
	return (both);
}

Var* ff_rgb2hsv(vfuncptr func, Var* arg)
{
	Var *obj = NULL, *maxval = NULL;
	float* data;
	double mval;
	int x, y, z, i, j, k1, k2, k3;

	RGB a;
	HSV b;

	Alist alist[3];
	alist[0]      = make_alist("object", ID_VAL, NULL, &obj);
	alist[1]      = make_alist("maxval", ID_VAL, NULL, &maxval);
	alist[2].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (obj == NULL) {
		parse_error("%s: No object specified\n", func->name);
		return (NULL);
	}

	if (maxval == NULL) {
		switch (V_FORMAT(obj)) {
		case DV_UINT8: mval  = (1 << 8) - 1; break;
		case DV_INT16: mval  = SHRT_MAX; break;
		case DV_INT32: mval  = INT_MAX; break;
		case DV_FLOAT: mval  = 1.0; break;
		case DV_DOUBLE: mval = 1.0; break;
		}
	} else {
		mval = extract_double(maxval, 0);
	}

	x = GetSamples(V_SIZE(obj), V_ORG(obj));
	y = GetLines(V_SIZE(obj), V_ORG(obj));
	z = GetBands(V_SIZE(obj), V_ORG(obj));

	if (z != 3) {
		parse_error("%s: Input must have 3 bands.\n", func->name);
		return (NULL);
	}

	data = (float*)calloc(4, 3 * x * y);
	for (i = 0; i < y; i++) {
		for (j = 0; j < x; j++) {
			k1 = cpos(j, i, 0, obj);
			k2 = cpos(j, i, 1, obj);
			k3 = cpos(j, i, 2, obj);

			a.r = extract_double(obj, k1) / mval;
			a.g = extract_double(obj, k2) / mval;
			a.b = extract_double(obj, k3) / mval;

			b = RGBToHSV(a);

			data[k1] = b.h;
			data[k2] = b.s;
			data[k3] = b.v;
		}
	}
	return (newVal(V_ORG(obj), V_SIZE(obj)[0], V_SIZE(obj)[1], V_SIZE(obj)[2], DV_FLOAT, data));
}

Var* ff_hsv2rgb(vfuncptr func, Var* arg)
{
	Var* obj = NULL;
	float* data;
	double mval = 1.0;
	int x, y, z, i, j, k1, k2, k3;

	HSV a;
	RGB b;

	Alist alist[3];
	alist[0]      = make_alist("object", ID_VAL, NULL, &obj);
	alist[1]      = make_alist("maxval", DV_DOUBLE, NULL, &mval);
	alist[2].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (obj == NULL) {
		parse_error("%s: No object specified\n", func->name);
		return (NULL);
	}

	x = GetSamples(V_SIZE(obj), V_ORG(obj));
	y = GetLines(V_SIZE(obj), V_ORG(obj));
	z = GetBands(V_SIZE(obj), V_ORG(obj));

	if (z != 3) {
		parse_error("%s: Input must have 3 bands.\n", func->name);
		return (NULL);
	}

	data = (float*)calloc(4, 3 * x * y);
	for (i = 0; i < y; i++) {
		for (j = 0; j < x; j++) {
			k1 = cpos(j, i, 0, obj);
			k2 = cpos(j, i, 1, obj);
			k3 = cpos(j, i, 2, obj);

			a.h = extract_double(obj, k1);
			a.s = extract_double(obj, k2);
			a.v = extract_double(obj, k3);

			b = HSVToRGB(a);

			data[k1] = b.r * mval;
			data[k2] = b.g * mval;
			data[k3] = b.b * mval;
		}
	}
	return (newVal(V_ORG(obj), V_SIZE(obj)[0], V_SIZE(obj)[1], V_SIZE(obj)[2], DV_FLOAT, data));
}

HSV RGBToHSV(RGB rgb)
{
	HSV hsv;
	float mn, mx;
	float rc, gc, bc;

	mx    = max(max(rgb.r, rgb.g), rgb.b);
	mn    = min(min(rgb.r, rgb.g), rgb.b);
	hsv.v = mx;
	if (mx == 0.0)
		hsv.s = 0.0;
	else
		hsv.s = (mx - mn) / mx;
	if (hsv.s == 0.0)
		hsv.h = 0.0;
	else {
		rc = (mx - rgb.r) / (mx - mn);
		gc = (mx - rgb.g) / (mx - mn);
		bc = (mx - rgb.b) / (mx - mn);
		if (rgb.r == mx)
			hsv.h = bc - gc;
		else if (rgb.g == mx)
			hsv.h = 2.0 + rc - bc;
		else if (rgb.b == mx)
			hsv.h = 4.0 + gc - rc;

		if (hsv.h < 0.0) hsv.h += 6.0;
		hsv.h = hsv.h / 6.0;
	}
	return hsv;
}

RGB HSVToRGB(HSV hsv)
{
	RGB rgb;
	float p, q, t, f;
	int i;

	if (hsv.s == 0.0)
		rgb.r = rgb.b = rgb.g = hsv.v;
	else {
		if (hsv.s > 1.0) hsv.s = 1.0;
		if (hsv.s < 0.0) hsv.s = 0.0;
		if (hsv.v > 1.0) hsv.v = 1.0;
		if (hsv.v < 0.0) hsv.v = 0.0;
		while (hsv.h >= 1.0) hsv.h -= 1.0;

		hsv.h = 6.0 * hsv.h;
		i     = (int)hsv.h;
		f     = hsv.h - (float)i;
		p     = hsv.v * (1.0 - hsv.s);
		q     = hsv.v * (1.0 - (hsv.s * f));
		t     = hsv.v * (1.0 - (hsv.s * (1.0 - f)));

		switch (i) {
		case 0:
			rgb.r = hsv.v;
			rgb.g = t;
			rgb.b = p;
			break;
		case 1:
			rgb.r = q;
			rgb.g = hsv.v;
			rgb.b = p;
			break;
		case 2:
			rgb.r = p;
			rgb.g = hsv.v;
			rgb.b = t;
			break;
		case 3:
			rgb.r = p;
			rgb.g = q;
			rgb.b = hsv.v;
			break;
		case 4:
			rgb.r = t;
			rgb.g = p;
			rgb.b = hsv.v;
			break;
		case 5:
			rgb.r = hsv.v;
			rgb.g = p;
			rgb.b = q;
			break;
		}
	}
	return rgb;
}

#define MAX_INTENSITY 255

RGB CMYToRGB(cmy) CMY cmy;

{
	RGB rgb;

	rgb.r = MAX_INTENSITY - cmy.c;
	rgb.g = MAX_INTENSITY - cmy.m;
	rgb.b = MAX_INTENSITY - cmy.y;
	return rgb;
}

/*
 * Convert an RGB to CMY.
 */

CMY RGBToCMY(rgb) RGB rgb;

{
	CMY cmy;

	cmy.c = MAX_INTENSITY - rgb.r;
	cmy.m = MAX_INTENSITY - rgb.g;
	cmy.y = MAX_INTENSITY - rgb.b;
	return cmy;
}

KCMY RGBtoKCMY(RGB rgb)
{
	KCMY kcmy;

	kcmy.c = MAX_INTENSITY - rgb.r;
	kcmy.m = MAX_INTENSITY - rgb.g;
	kcmy.y = MAX_INTENSITY - rgb.b;

	kcmy.k = min(min(kcmy.c, kcmy.m), kcmy.y);

	if (kcmy.k > 0) {
		kcmy.c = kcmy.c - kcmy.k;
		kcmy.m = kcmy.m - kcmy.k;
		kcmy.y = kcmy.y - kcmy.k;
	}
	return (kcmy);
}

RGB KCMYtoRGB(KCMY kcmy)
{
	RGB rgb;

	rgb.r = MAX_INTENSITY - (kcmy.c + kcmy.k);
	rgb.g = MAX_INTENSITY - (kcmy.m + kcmy.k);
	rgb.b = MAX_INTENSITY - (kcmy.y + kcmy.k);

	return (rgb);
}

/**
 ** Compute entropy of an image
 **/

Var* ff_entropy(vfuncptr func, Var* arg)
{
	Var* obj = NULL;
	int i, dsize, count;
	void *a, *b, *data;
	int format, nbytes;
	float p, ent = 0;
	int (*cmp)(const void*, const void*);

	Alist alist[2];
	alist[0]      = make_alist("object", ID_VAL, NULL, &obj);
	alist[1].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (obj == NULL) {
		parse_error("%s: No object specified\n", func->name);
		return (NULL);
	}

	dsize  = V_DSIZE(obj);
	format = V_FORMAT(obj);
	nbytes = NBYTES(V_FORMAT(obj));
	data   = memdup(V_DATA(obj), dsize * NBYTES(V_FORMAT(obj)));

	switch (format) {
	case DV_UINT8: cmp  = cmp_u8; break;
	case DV_UINT16: cmp = cmp_u16; break;
	case DV_UINT32: cmp = cmp_u32; break;
	case DV_UINT64: cmp = cmp_u64; break;
	case DV_INT8: cmp   = cmp_i8; break;
	case DV_INT16: cmp  = cmp_i16; break;
	case DV_INT32: cmp  = cmp_i32; break;
	case DV_INT64: cmp  = cmp_i64; break;
	case DV_FLOAT: cmp  = cmp_float; break;
	case DV_DOUBLE: cmp = cmp_double; break;
	}
	qsort(data, V_DSIZE(obj), NBYTES(format), cmp);

	a     = data;
	count = 0;
	for (i = 0; i < dsize; i++) {
		b = ((char*)a) + nbytes;
		if (!cmp(a, b) && (i + 1) < dsize) {
			count++;
		} else {
			p = (float)(count + 1) / (float)dsize;
			ent += p * log(p) / M_LN2;
			count = 0;
		}
		a = b;
	}
	ent = -ent;
	free(data);
	return (newVal(BSQ, 1, 1, 1, DV_FLOAT, memdup(&ent, sizeof(DV_FLOAT))));
}
