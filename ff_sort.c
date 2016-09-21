#include "parser.h"

static void quicksort(void*, size_t, size_t, int (*)(), size_t*, int, int);
static void* reorgByIndex(Var*, Var*, size_t*);


#define cmp_func_asc(type) \
int cmp_##type(const void* a, const void* b) \
{ \
	if (*(type*)a > *(type*)b) return 1; \
	if (*(type*)a < *(type*)b) return -1; \
	return 0; \
}

#define cmp_func_dsc(type) \
int cmp_##type##_dsc(const void* a, const void* b) \
{ \
	if (*(type*)a > *(type*)b) return -1; \
	if (*(type*)a < *(type*)b) return 1; \
	return 0; \
}


cmp_func_asc(u8)
cmp_func_asc(u16)
cmp_func_asc(u32)
cmp_func_asc(u64)

cmp_func_asc(i8)
cmp_func_asc(i16)
cmp_func_asc(i32)
cmp_func_asc(i64)

cmp_func_asc(float)
cmp_func_asc(double)

cmp_func_dsc(u8)
cmp_func_dsc(u16)
cmp_func_dsc(u32)
cmp_func_dsc(u64)

cmp_func_dsc(i8)
cmp_func_dsc(i16)
cmp_func_dsc(i32)
cmp_func_dsc(i64)

cmp_func_dsc(float)
cmp_func_dsc(double)

int cmp_string(const void* a, const void* b)
{
	return strcmp(*(const char**)a, *(const char**)b);
}

int cmp_string_dsc(const void* a, const void* b)
{
	return strcmp(*(const char**)b, *(const char**)a);
}


// Move data [from] object, [to] data, using size to expand the offset as needed
#define reorg(to, data, from, object, size) \
	memcpy((void*)((char*)data + to * size), (void*)((char*)object + from * size), size);

/*Reorganizes the object, using the index object and its respective sortList.
 * object - Data that is being manipulated
 * index - Source for the sorted sortList
 * sortList  - Sorted sortList, describing where blocks of object are placed
 */
static void* reorgByIndex(Var* object, Var* index, size_t* sortList)
{
	int x, y, z;            // x y z for object
	int i, j, k;            // Loop counters;  i - x, j - y, k - z
	int doX = 1;            // Determines if
	int doY = 1;            // X, Y, or Z
	int doZ = 1;            // axis points remain contiguous
	int from, to;           // from / to, memory offsets for cpos
	int format         = 0; // object's format
	size_t dsize       = 0; // object's size
	size_t sortListNum = 0; // index and sortList's size
	int cntr           = 0; // sortList counter
	void* data;             // Returned data block

	x = GetX(object);
	y = GetY(object);
	z = GetZ(object);

	// Any axis on index with a depth of '1' remains in a contiguous block
	// if the axis is greater than '1', it is not contiguous
	if (GetX(index) > 1) doX = 0;
	if (GetY(index) > 1) doY = 0;
	if (GetZ(index) > 1) doZ = 0;

	// If object and index do not share a similar depth on its axis
	//  then memcpy could reach outside the allocated memory.
	// Avoid this by comparing the depth for axis that were sorted.
	if ((!doX && x != GetX(index)) || (!doY && y != GetY(index)) || (!doZ && z != GetZ(index)))
		return NULL;

	format      = V_FORMAT(object);
	dsize       = V_DSIZE(object);
	sortListNum = V_DSIZE(index);

	// Alocate the data
	data = calloc(dsize, NBYTES(format));

	// sort(object, index=[1,1, ])
	if (doX && doY)
		for (k = 0; k < sortListNum; k++)
			for (i = 0; i < x; i++)
				for (j = 0; j < y; j++) {
					to   = cpos(i, j, k, object);
					from = cpos(i, j, sortList[k], object);

					reorg(to, data, from, V_DATA(object), NBYTES(format));
				}
	// sort(object, index=[1, ,1])
	else if (doX && doZ)
		for (j = 0; j < sortListNum; j++)
			for (i = 0; i < x; i++)
				for (k = 0; k < z; k++) {
					to   = cpos(i, j, k, object);
					from = cpos(i, sortList[j], k, object);

					reorg(to, data, from, V_DATA(object), NBYTES(format));
				}
	// sort(object, index=[ ,1,1])
	else if (doY && doZ)
		for (i = 0; i < sortListNum; i++)
			for (j = 0; j < y; j++)
				for (k = 0; k < z; k++) {
					to   = cpos(i, j, k, object);
					from = cpos(sortList[i], j, k, object);

					reorg(to, data, from, V_DATA(object), NBYTES(format));
				}
	// sort(object, index=[1, , ])
	else if (doX)
		for (cntr = 0; cntr < sortListNum; cntr++)
			for (i = 0; i < x; i++) {
				to   = cpos(i, cntr % y, cntr / y, object);
				from = cpos(i, sortList[cntr] % y, sortList[cntr] / y, object);

				reorg(to, data, from, V_DATA(object), NBYTES(format));
			}
	// sort(object, index=[ ,1, ])
	else if (doY)
		for (cntr = 0; cntr < sortListNum; cntr++)
			for (j = 0; j < y; j++) {
				to   = cpos(cntr % x, j, cntr / x, object);
				from = cpos(sortList[cntr] % x, j, sortList[cntr] / x, object);

				reorg(to, data, from, V_DATA(object), NBYTES(format));
			}
	// sort(object, index=[ , ,1])
	else if (doZ)
		for (cntr = 0; cntr < sortListNum; cntr++)
			for (k = 0; k < z; k++) {
				to   = cpos(cntr % x, cntr / x, k, object);
				from = cpos(sortList[cntr] % x, sortList[cntr] / x, k, object);

				reorg(to, data, from, V_DATA(object), NBYTES(format));
			}

	return data;
}

// TODO(rswinkle) Figure out some way to use the standard library qsort instead.
// There's got to be a way.  We could just sort the user's data directly which would
// make in easy.

//	QuickSort adapted from Kernighan and Ritchie, 'The C Programming Language'
static inline void qswap(void* base, int i, int j, int width, size_t* sortList)
{
	long tmp;
	unsigned char b;
	int k;
	char* basec = base;

	// Maintain a sorted list of elements
	tmp         = sortList[j];
	sortList[j] = sortList[i];
	sortList[i] = tmp;

	// Swap two elements, given the size of the elements
	for (k = 0; k < width; k++) {
		b                    = basec[i * width + k];
		basec[i * width + k] = basec[j * width + k];
		basec[j * width + k] = b;
	}
}

//	QuickSort adapted from Kernighan and Ritchie, 'The C Programming Language'
static void quicksort(void* base, size_t num, size_t width, int (*cmp)(), size_t* sortList,
                      int left, int right)
{
	int i, last;

	if (left > right) return;

	qswap(base, left, (left + right) / 2, width, sortList);

	last = left;

	for (i = left + 1; i <= right; i++)
		if (cmp(base + i * width, base + left * width) < 0)
			qswap(base, (++last), i, width, sortList);

	qswap(base, left, last, width, sortList);
	quicksort(base, num, width, cmp, sortList, left, last - 1);
	quicksort(base, num, width, cmp, sortList, last + 1, right);
}

static int check_sort_objects(Var* object, Var* byObj)
{
	int Ox = 0, Oy = 0, Oz = 0;
	int Bx = 0, By = 0, Bz = 0;
	int i = 0;

	if (V_TYPE(object) == ID_VAL) {
		Ox = GetX(object);
		Oy = GetY(object);
		Oz = GetZ(object);
	}

	if (V_TYPE(object) == ID_TEXT) {
		Ox = 1;
		Oy = V_TEXT(object).Row;
		Oz = 1;
	}

	if (V_TYPE(byObj) == ID_VAL) {
		Bx = GetX(byObj);
		By = GetY(byObj);
		Bz = GetZ(byObj);
	}

	if (V_TYPE(byObj) == ID_TEXT) {
		Bx = 1;
		By = V_TEXT(byObj).Row;
		Bz = 1;
	}

	if ((Bx != Ox && Bx != 1) || (By != Oy && By != 1) || (Bz != Oz && Bz != 1) || (Bx * By * Bz == 1)) {
		parse_error("Dimensions of \'object\' and \'by\' are incompatible");
		i = -1;
	}

	return (i);
}

Var* ff_sort(vfuncptr func, Var* arg)
{
	Var* object       = NULL;
	Var* sortVar      = NULL;
	Var* byObj        = NULL;
	Var* args         = NULL;
	Var* result       = NULL;
	char* oneline     = NULL;
	char** tlines     = NULL;
	size_t* indexList = NULL;
	int format;
	size_t dsize;
	size_t i, j;
	int rows      = 0;
	int descend = 0;
	void *data, *sortSet;
	int (*cmp)(const void*, const void*);

	Alist alist[4];
	alist[0]      = make_alist("object", ID_UNK, NULL, &object);
	alist[1]      = make_alist("by", ID_UNK, NULL, &byObj);
	alist[2]      = make_alist("descend", DV_INT32, NULL, &descend);
	alist[3].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (object == NULL) {
		parse_error("\nNo object specified\n");
		return NULL;
	}

	/* Check object and byObj are compatible sizes */
	if (byObj) {
		i = check_sort_objects(object, byObj);
		if (i == -1) return (NULL);
	}

	/* Assign sortVar */
	sortVar            = object;
	if (byObj) sortVar = byObj;

	if (V_TYPE(sortVar) == ID_VAL) {

		format = V_FORMAT(sortVar);
		dsize  = V_DSIZE(sortVar);

		switch (format) {
		case DV_UINT8:  cmp = (descend) ? cmp_u8_dsc : cmp_u8; break;
		case DV_UINT16: cmp = (descend) ? cmp_u16_dsc : cmp_u16; break;
		case DV_UINT32: cmp = (descend) ? cmp_u32_dsc : cmp_u32; break;
		case DV_UINT64: cmp = (descend) ? cmp_u64_dsc : cmp_u64; break;

		case DV_INT8:  cmp  = (descend) ? cmp_i8_dsc : cmp_i8; break;
		case DV_INT16: cmp  = (descend) ? cmp_i16_dsc : cmp_i16; break;
		case DV_INT32: cmp  = (descend) ? cmp_i32_dsc : cmp_i32; break;
		case DV_INT64: cmp  = (descend) ? cmp_i64_dsc : cmp_i64; break;

		case DV_FLOAT: cmp  = (descend) ? cmp_float_dsc : cmp_float; break;
		case DV_DOUBLE: cmp = (descend) ? cmp_double_dsc : cmp_double; break;
		}

		if (V_ORG(sortVar) != BSQ) {
			args    = create_args(1, NULL, sortVar, NULL, NULL);
			sortVar = V_func("bsq", args);
		}

		/* Create memory for sortSet -- avoid touching the user's data */
		sortSet = calloc(dsize, NBYTES(format));
		memcpy(sortSet, V_DATA(sortVar), dsize * NBYTES(format));

		/* sortList will record the index of items that are moved */
		indexList = calloc(dsize, sizeof(size_t));
		for (i = 0; i < dsize; i++) indexList[i] = i;

		/* Sort and create indexList */
		if (dsize > 0) quicksort(sortSet, dsize, NBYTES(format), cmp, indexList, 0, dsize - 1);

		if (byObj == NULL) {
			free(sortSet);

			// parse_error("Object = ID_VAL, byObj = NULL");
			// parse_error("works");
			data   = reorgByIndex(object, object, indexList);
			result = newVal(V_ORG(object), V_SIZE(object)[0], V_SIZE(object)[1], V_SIZE(object)[2],
			                format, data);

			free(indexList);
			return (result);

		} else {

			free(sortSet);

			if (V_TYPE(object) == ID_VAL) {
				// parse_error("Object = ID_VAL, byObj = ID_VAL");
				// parse_error("works");
				data = reorgByIndex(object, byObj, indexList);

				result = newVal(V_ORG(object), V_SIZE(object)[0], V_SIZE(object)[1],
				                V_SIZE(object)[2], V_FORMAT(object), data);
				free(indexList);
				return (result);

			} else if (V_TYPE(object) == ID_TEXT) {
				// parse_error("Object = ID_TEXT, byObj = ID_VAL");
				// parse_error("works");
				rows   = V_TEXT(object).Row;
				tlines = (char**)calloc(rows, sizeof(char*));

				for (i = 0; i < rows; i += 1) {
					j         = indexList[i];
					tlines[i] = strdup(V_TEXT(object).text[j]);
				}
				result = newText(rows, tlines);
				free(indexList);
				return (result);
			}
		}
	} else if (V_TYPE(sortVar) == ID_TEXT) {

		cmp    = cmp_string;
		rows   = V_TEXT(sortVar).Row;
		tlines = (char**)calloc(rows, sizeof(char*));

		for (i = 0; i < rows; i++) {
			tlines[i]                        = strdup(V_TEXT(sortVar).text[i]);
			if (tlines[i] == NULL) tlines[i] = strdup("");
		}

		indexList = calloc(rows, sizeof(size_t));

		for (i = 0; i < rows; i++) indexList[i] = i;

		quicksort(tlines, rows, sizeof(char*), cmp_string, indexList, 0, rows - 1);

		if (byObj == NULL) {
			// parse_error("Object = ID_TEXT, byObj = NULL");
			// parse_error("works");
			free(indexList);
			result = newText(rows, tlines);
			return (result);

		} else {
			if (V_TYPE(object) == ID_VAL) {
				// parse_error("Object = ID_VAL, byObj = ID_TEXT");
				// parse_error("works, but has possible memory leak");
				args   = newVal(BSQ, 1, rows, 1, DV_INT32, indexList);
				data   = reorgByIndex(object, args, indexList);
				result = newVal(V_ORG(object), V_SIZE(object)[0], V_SIZE(object)[1],
				                V_SIZE(object)[2], V_FORMAT(object), data);
				// free(indexList);
				return (result);

			} else if (V_TYPE(object) == ID_TEXT) {
				// parse_error("Object = ID_TEXT, byObj = ID_TEXT");
				// parse_error("works");
				rows = V_TEXT(object).Row;

				for (i = 0; i < rows; i += 1) {
					j         = indexList[i];
					tlines[i] = strdup(V_TEXT(object).text[j]);
				}
				result = newText(rows, tlines);
				free(indexList);
				return (result);
			}
		}

	} else {
		parse_error("Incompatible object type. Must be TEXT or VAL.");
	}
	return NULL;
}

Var* ff_unique(vfuncptr func, Var* arg)
{
	Var* object     = NULL; /* the object */
	Var* byObj      = NULL; /* optional object to find unique elements in */
	Var* searchVar  = NULL;
	Var* result     = NULL;
	char** uniqText = NULL;
	float* uniqData = NULL;
	int* indexList  = NULL;
	int x, y, z;
	int sx, sy, sz;
	int i, j, k, l; /* general array counters */
	int uniqElems = 0;
	float m;

	Alist alist[3];
	alist[0]      = make_alist("object", ID_UNK, NULL, &object);
	alist[1]      = make_alist("by", ID_UNK, NULL, &byObj);
	alist[2].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (object == NULL) {
		parse_error("\nNo data specified\n");
		return NULL;
	}

	/* Check object and byObj are compatible sizes */
	if (byObj) {
		i = check_sort_objects(object, byObj);
		if (i == -1) return (NULL);
	}

	searchVar            = object;
	if (byObj) searchVar = byObj;

	if (V_TYPE(searchVar) == ID_TEXT) {
		sx = 1;
		sy = V_TEXT(searchVar).Row;
		sz = 1;

		indexList = malloc(sizeof(int) * sy);
		for (j = 0; j < sy; j += 1) indexList[j] = 1;

		/* Compare all elements marking duplicates with a 0 in indexList */
		for (j = 0; j < sy; j += 1) {
			if (indexList[j] == 1) {
				for (i = j + 1; i < sy; i += 1)
					if (strcmp(V_TEXT(searchVar).text[j], V_TEXT(searchVar).text[i]) == 0)
						indexList[i] = 0;
			}
		}

	} else if (V_TYPE(searchVar) == ID_VAL) {
		sx = GetX(searchVar);
		sy = GetY(searchVar);
		sz = GetZ(searchVar);

		k         = sx * sy * sz;
		indexList = calloc(sizeof(int), k);
		for (i = 0; i < (sx * sy * sz); i += 1) indexList[i] = 1;

		/* Compare all elements marking duplicates with a 0 in indexList */
		for (k = 0; k < sz * sy * sx; k += 1) {
			if (indexList[k] == 1) {
				m = extract_float(searchVar, k);
				for (j = k + 1; j < sz * sy * sx; j += 1)
					if (m == extract_float(searchVar, j)) indexList[j] = 0;
			}
		}

		if (!byObj) {
			uniqElems = 0;
			for (i = 0; i < sx * sy * sz; i += 1) uniqElems = uniqElems + indexList[i];

			uniqData = calloc(uniqElems, sizeof(float));

			j = 0;
			for (k = 0; k < sz * sy * sx; k++) {
				if (indexList[k] == 1) {
					uniqData[j] = extract_float(searchVar, k);
					j++;
				}
			}

			result = newVal(BSQ, 1, uniqElems, 1, DV_FLOAT, uniqData);
			free(indexList);
			return (result);
		}

	} else {
		parse_error("Incompatible variable type! Only TEXT or VAL allowed.");
		return (NULL);
	}

	if (V_TYPE(object) == ID_TEXT) {

		uniqElems = 0;
		for (i = 0; i < sy; i += 1) uniqElems = uniqElems + indexList[i];

		uniqText = (char**)calloc(uniqElems, sizeof(char*));

		j = 0;
		for (i = 0; i < sy; i++) {
			if (indexList[i] == 1) {
				uniqText[j] = strdup(V_TEXT(object).text[i]);
				j++;
			}
		}

		result = newText(uniqElems, uniqText);
		free(indexList);
		return (result);

	} else if (V_TYPE(object) == ID_VAL) {

		k = sx * sy * sz;

		if (k / sz != 1 && k / sy != 1 && k / sx != 1) {
			parse_error("Error: \'by\' object must be 1 dimensional.");
			return (NULL);
		}

		x = GetX(object);
		y = GetY(object);
		z = GetZ(object);

		uniqElems = 0;
		for (i = 0; i < k; i += 1) uniqElems = uniqElems + indexList[i];

		if (sx != 1) {
			uniqData = calloc(uniqElems * y * z, NBYTES(V_FORMAT(object)));

			l = -1;
			for (i = 0; i < x; i++) {
				if (indexList[i] == 1) {
					l += 1;
					for (k = 0; k < z; k++) {
						for (j = 0; j < y; j++) {
							uniqData[k * y * uniqElems + j * uniqElems + l] =
							    extract_float(object, cpos(i, j, k, object));
						}
					}
				}
			}
			result = newVal(BSQ, uniqElems, y, z, DV_FLOAT, uniqData);
			return (result);

		} else if (sy != 1) {
			uniqData = calloc(uniqElems * x * z, NBYTES(V_FORMAT(object)));

			l = -1;
			for (j = 0; j < y; j++) {
				if (indexList[j] == 1) {
					l += 1;
					for (k = 0; k < z; k++) {
						for (i = 0; i < x; i++) {
							uniqData[k * uniqElems * x + l * x + i] =
							    extract_float(object, cpos(i, j, k, object));
						}
					}
				}
			}
			result = newVal(BSQ, x, uniqElems, z, DV_FLOAT, uniqData);
			return (result);

		} else if (sz != 1) {
			uniqData = calloc(uniqElems * x * z, NBYTES(V_FORMAT(object)));

			l = -1;
			for (k = 0; k < z; k++) {
				if (indexList[k] == 1) {
					l += 1;
					for (j = 0; j < y; j++) {
						for (i = 0; i < x; i++) {
							uniqData[l * y * x + j * x + i] =
							    extract_float(object, cpos(i, j, k, object));
						}
					}
				}
			}
			result = newVal(BSQ, x, y, uniqElems, DV_FLOAT, uniqData);
			return (result);
		}
	}
	return NULL;
}

