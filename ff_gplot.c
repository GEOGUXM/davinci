#include "parser.h"

// This is probably overriden in config.h
#ifndef GPLOT_CMD
#define GPLOT_CMD "gnuplot"
#endif

FILE* gplot_pfp = NULL;

Var* ff_gplot(vfuncptr func, Var* arg)
{
	char* gplot_cmd;
	Var* object = NULL;
	int i;
	FILE* fp      = NULL;
	char* fname   = NULL;
	float g_xlow  = FLT_MAX;
	float g_xhigh = FLT_MAX;
	float g_ylow  = FLT_MAX;
	float g_yhigh = FLT_MAX;

	Alist alist[6];
	alist[0]      = make_alist("object", ID_VAL, NULL, &object);
	alist[1]      = make_alist("xlow", DV_FLOAT, NULL, &g_xlow);
	alist[2]      = make_alist("xhigh", DV_FLOAT, NULL, &g_xhigh);
	alist[3]      = make_alist("ylow", DV_FLOAT, NULL, &g_ylow);
	alist[4]      = make_alist("yhigh", DV_FLOAT, NULL, &g_yhigh);
	alist[5].name = NULL;

	if (parse_args(func, arg, alist) == 0) return (NULL);

	if (object == NULL || V_TYPE(object) != ID_VAL) {
		fprintf(stderr, "Expected value for keyword: object\n");
		return (NULL);
	}

	if (gplot_pfp == NULL) {
		if (getenv("GPLOT_CMD") != NULL) {
			gplot_cmd = strdup(getenv("GPLOT_CMD"));
		} else {
			gplot_cmd = GPLOT_CMD;
		}
		if ((gplot_pfp = popen(gplot_cmd, "w")) == NULL) {
			fprintf(stderr, "Unable to open gplot.\n");
			return (0);
		}
	}
	fname = make_temp_file_path();
	if (fname == NULL || (fp = fopen(fname, "w")) == NULL) {
		parse_error("%s: unable to open temp file", func->name);
		if (fname) free(fname);
		return (NULL);
	}

	// NOTE(rswinkle): Assuming it's a valid type simplifies checking
	// and we never handled invalid types before anyway
	if (V_FORMAT(object) <= DV_INT64) {
		for (i = 0; i < V_DSIZE(object); i++)
			fprintf(fp, "%d\n", extract_int(object, i));
	} else {
		for (i = 0; i < V_DSIZE(object); i++)
			fprintf(fp, "%g\n", extract_double(object, i));
	}

	fclose(fp);

	fprintf(gplot_pfp, "plot ");
	fprintf(gplot_pfp, "[ ");
	if (g_xlow != FLT_MAX) fprintf(gplot_pfp, "%g", g_xlow);
	fprintf(gplot_pfp, ":");
	if (g_xhigh != FLT_MAX) fprintf(gplot_pfp, "%g", g_xhigh);
	fprintf(gplot_pfp, "] ");

	fprintf(gplot_pfp, "[ ");
	if (g_ylow != FLT_MAX) fprintf(gplot_pfp, "%g", g_ylow);
	fprintf(gplot_pfp, ":");
	if (g_yhigh != FLT_MAX) fprintf(gplot_pfp, "%g", g_yhigh);
	fprintf(gplot_pfp, "] '%s' with linespoints\n", fname);
	fflush(gplot_pfp);

	free(fname);
	return (NULL);
}

Var* ff_splot(vfuncptr func, Var* arg)
{
#ifdef HAVE_LIBX11
	Var *s = NULL, *v = NULL;
	FILE* fp;
	char* fname = NULL;
	size_t i, j;
	int s0, s1;
	int type;
	char buf[512];
	int count   = 0;
	int pm3d    = 0;
	char* label = NULL;
	int ac;
	Var** av;
	float xscale = 0;
	float yscale = 0;

	make_args(&ac, &av, func, arg);
	/* make_args puts func->name into av[0] and ac == 2 */
	ac--;

	if (ac == 0) {
		parse_error("\nNo data specified\n");
		return (NULL);
	}

	if (ac == 2 && V_TYPE(av[1]) == ID_STRING) {
		strcpy(buf, V_STRING(av[1]));

	} else {
		strcpy(buf, "splot ");
		for (j = 1; j <= ac; j++) {
			s = av[j];

			if (V_TYPE(s) == ID_STRING) {
				label = strdup(V_STRING(s));

			} else if (V_TYPE(s) == ID_KEYWORD) {

				if (!strcasecmp(V_NAME(s), "pm3d")) pm3d = 1;

				if (!strcasecmp(V_NAME(s), "label")) {
					if (V_KEYVAL(s) != NULL) label = strdup(V_NAME(V_KEYVAL(s)));
				}

				if (!strcasecmp(V_NAME(s), "xscale")) {
					if (V_KEYVAL(s) != NULL) {
						if (V_TYPE(V_KEYVAL(s)) == ID_VAL) {
							v = V_KEYVAL(s);
							xscale = extract_float(v, 0);
						}
					}
				}

				if (!strcasecmp(V_NAME(s), "yscale")) {
					if (V_KEYVAL(s) != NULL) {
						if (V_TYPE(V_KEYVAL(s)) == ID_VAL) {
							v = V_KEYVAL(s);
							yscale = extract_float(v, 0);
						}
					}
				}
			}
		}

		for (j = 1; j <= ac; j++) {
			s = av[j];

			if (V_TYPE(s) == ID_STRING) {
			} else if (V_TYPE(s) == ID_KEYWORD) {
			} else {

				if ((v = eval(s)) == NULL) v = s;

				type = V_FORMAT(v);

				fname = make_temp_file_path();
				if (fname == NULL || (fp = fopen(fname, "w")) == NULL) {
					parse_error("%s: unable to open temp file", func->name);
					if (fname) free(fname);
					return (NULL);
				}

				s0 = V_SIZE(v)[0];
				s1 = V_SIZE(v)[1] * V_SIZE(v)[0];
				for (i = 0; i < V_DSIZE(v); i++) {
					if ((i && (i % s0) == 0) || s0 == 1) fputc('\n', fp);

					// NOTE(rswinkle): technically a high enough u64 could produce negative extracted as i64
					// but I think i64 max is sufficient and we could alway break the unsigned types out if
					// we wanted
					if (type <= DV_INT64) {
						if (xscale != 0 && yscale != 0) {
							fprintf(fp, "%ld %ld %ld\n", ((i % s0) + 1) * (i64)xscale,
							        ((i / s0) + 1) * (i64)yscale, extract_i64(v, i));
						} else {
							fprintf(fp, "%zu %zu %ld\n", (i % s0) + 1, (i / s0) + 1, extract_i64(v, i));
						}
					} else {
						if (xscale != 0 && yscale != 0) {
							fprintf(fp, "%g %g %.12g\n", (int)((i % s0) + 1) * xscale,
							        (int)((i / s0) + 1) * yscale, extract_float(v, i));
						} else {
							fprintf(fp, "%zu %zu %.12g\n", (i % s0) + 1, (i / s0) + 1, extract_float(v, i));
						}
					}
				}
				fclose(fp);
				if (count++) strcat(buf, ",");
				sprintf(buf + strlen(buf), "\"%s\"", fname);
				if (label != NULL) sprintf(buf + strlen(buf), "title '%s'", label);
				if (pm3d != 0) sprintf(buf + strlen(buf), " with pm3d");
				free(fname);
			}
		}
	}
	send_to_plot(buf);
#endif
	return (NULL);
}

int send_to_plot(const char* s)
{
#ifdef GPLOT_CMD
	char* gplot_cmd;

	if (pfp == NULL) {
		if (getenv("GPLOT_CMD") != NULL) {
			gplot_cmd = strdup(getenv("GPLOT_CMD"));
		} else {
			gplot_cmd = GPLOT_CMD;
		}
		if ((pfp = popen(gplot_cmd, "w")) == NULL) {
			fprintf(stderr, "Unable to open gplot.\n");
			return (0);
		}
		send_to_plot("set data style linespoints\n");
		send_to_plot("set parametric\n");
		send_to_plot("set mouse\n");

#ifdef HAVE_AQUA
		if (getenv("DV_AQUA")) {
			char* have_aqua = strdup(getenv("DV_AQUA"));
			if (have_aqua != NULL && strcmp(have_aqua, "1") == 0) {
				send_to_plot("set term aqua\n");
			}
		}
#endif
	}
	if (write(fileno(pfp), s, strlen(s)) < 0) {
		pfp = NULL;
		send_to_plot(s);
	}
	write(fileno(pfp), "\n", 1);
#endif /* GPLOT_CMD */
	return (1);
}

void Find_Axis(char* R, Var* Obj)
{
	int axis = 0;

	if (GetSamples(V_SIZE(Obj), V_ORG(Obj)) == 1) axis |= 1;
	if (GetLines(V_SIZE(Obj), V_ORG(Obj)) == 1) axis |= 2;
	if (GetBands(V_SIZE(Obj), V_ORG(Obj)) == 1) axis |= 4;

	if (axis == 7) *R = '\0';

	if (axis == 6)
		*R = 'X';
	else if (axis == 5)
		*R = 'Y';
	else if (axis == 3)
		*R = 'Z';
	else
		*R = 'Y';
}

Var* ff_xplot(vfuncptr func, Var* arg)
{
	/*      Modified Plot; Created:
	 **   Mon Jul 31 10:21:29 MST 2000
	 **   --Ben
	 */

	Var *s, *v;
	FILE* fp;
	char* fname;

	char buf[5120] = {0}; /* added factor of 10 */
	int count      = 0;

	Var* Xaxis = NULL;

	int Mode[3];

	int Ord[3];
	int XOrd[3];

	int CE[3];

	int xFlag  = 0;
	char* Axis = NULL;
	int obj_index;
	float *x, *y;
	int i, j, k;
	int idx;
	int Sep = 0;

	char axs;

	int ac;
	int argcount = 0;
	Var** av;
	float ignore;
	int iflag = 0;

	make_args(&ac, &av, func, arg); /*chop up the args into an array */
	/* make_args puts func->name into av[0] and ac == 2 */

	for (i = 1; i <= ac; i++) {
		if (av[i] == NULL) continue;

		if (V_TYPE(av[i]) == ID_KEYWORD) {
			if (!(strcmp(av[i]->name, "Xaxis"))) {
				Xaxis = eval(V_KEYVAL(av[i]));
				if (Xaxis == NULL) {
					parse_error("Variable not found: Xaxis=...");
					free(av);
					return (NULL);
				}
			} else if (!(strcmp(av[i]->name, "axis"))) {
				v = V_KEYVAL(av[i]);
				if (V_TYPE(v) == ID_STRING)
					Axis = V_STRING(v);
				else
					Axis = V_NAME(v);

			} else if (!(strcmp(av[i]->name, "separate"))) {
				Sep = 1;
			} else if (!(strcmp(av[i]->name, "ignore"))) {
				if (V_KEYVAL(av[i]) != NULL) {
					ignore = extract_float(V_KEYVAL(av[i]), 0);
					iflag  = 1;
				} else {
					parse_error("Bad value for ignore");
					free(av);
					return (NULL);
				}
			} else {
				parse_error("Illegal keyword %s\n", av[i]->name);
				free(av);
				return (NULL);
			}
			av[i] = NULL;
		}
		argcount++;
	}

	if (Xaxis != NULL) xFlag = 1;

	if (Axis == NULL) {
		for (idx = 1; idx <= ac; idx++) {
			if (av[idx] == NULL) continue;

			s = av[idx];

			if (V_TYPE(s) == ID_UNK) {
				if ((s = eval(av[idx])) == NULL) {
					parse_error("Unknown Variable\n");
					free(av);
					return (NULL);
				}
			}
			if (V_TYPE(s) != ID_VAL) continue;

			Find_Axis(&axs, s);
			Axis = strdup(&axs);
			break;
		}
		if (Axis == NULL) {
			parse_error("xplot: No variables.\n");
			free(av);
			return (NULL);
		}
	}
	switch (*Axis) {
	case 'X':
	case 'x':
		Mode[0] = 0;
		Mode[1] = 1;
		Mode[2] = 2;
		break;
	case 'Y':
	case 'y':
		Mode[0] = 1;
		Mode[1] = 0;
		Mode[2] = 2;
		break;
	case 'Z':
	case 'z':
		Mode[0] = 2;
		Mode[1] = 0;
		Mode[2] = 1;
		break;
	default:
		Mode[0] = 1;
		Mode[1] = 0;
		Mode[2] = 2;
		break;
	}

	if (Xaxis != NULL) {
		XOrd[0] = GetSamples(V_SIZE(Xaxis), V_ORG(Xaxis));
		XOrd[1] = GetLines(V_SIZE(Xaxis), V_ORG(Xaxis));
		XOrd[2] = GetBands(V_SIZE(Xaxis), V_ORG(Xaxis));
	}

	if (argcount == 2 && V_TYPE(av[1]) == ID_STRING) {
		strcpy(buf, V_STRING(av[1]));
		send_to_plot(buf);
	} else {
		strcpy(buf, "plot ");
		for (idx = 1; idx <= ac; idx++) {
			s = av[idx];

			if (s == NULL) continue;

			if (V_TYPE(s) == ID_UNK) {
				if ((s = eval(av[idx])) == NULL) {
					parse_error("Unknown Variable\n");
					free(av);
					return (NULL);
				}
			}
			switch (V_TYPE(s)) {

			case ID_STRING:
				strcat(buf, V_STRING(s));
				count++;
				break;
			case ID_VAL:
				if (!(Sep)) {
					fname = make_temp_file_path();
					if (fname == NULL || (fp = fopen(fname, "w")) == NULL) {
						parse_error("%s: unable to open temp file", func->name);
						if (fname) free(fname);
						free(av);
						return (NULL);
					}
				}
				if ((v = eval(s)) == NULL) v = s;

				Ord[0] = GetSamples(V_SIZE(v), V_ORG(v));
				Ord[1] = GetLines(V_SIZE(v), V_ORG(v));
				Ord[2] = GetBands(V_SIZE(v), V_ORG(v));

				if (xFlag) {

					if (XOrd[Mode[0]] != Ord[Mode[0]]) {
						parse_error("Given X-Axis doesn't agree with given data set");
						free(av);
						return (NULL);
					} else if ((XOrd[1] != 1 && XOrd[2] != 1) &&
					           (XOrd[1] != Ord[Mode[1]] && XOrd[2] != Ord[Mode[2]])) {
						parse_error("Given X-Axis doesn't agree with given data set");
						free(av);
						return (NULL);
					}
				}
				x = calloc(Ord[Mode[0]], sizeof(float));
				y = calloc(Ord[Mode[0]], sizeof(float));

				for (i = 0; i < Ord[Mode[2]]; i++) {
					for (j = 0; j < Ord[Mode[1]]; j++) {

						if (Sep) {
							fname = make_temp_file_path();
							if (fname == NULL || (fp = fopen(fname, "w")) == NULL) {
								parse_error("%s: unable to open temp file", func->name);
								if (fname) {
									free(fname);
								}
								free(av);
								return (NULL);
							}
						}
						for (k = 0; k < Ord[Mode[0]]; k++) {
							CE[Mode[2]] = i;
							CE[Mode[1]] = j;
							CE[Mode[0]] = k;
							obj_index   = cpos(CE[0], CE[1], CE[2], v);

							y[k] = extract_float(v, obj_index);

							if (xFlag) {
								x[k] = extract_float(Xaxis, rpos(obj_index, v, Xaxis));
							} else {
								x[k] = (float)k;
							}
							if (iflag == 0 || (x[k] != ignore && y[k] != ignore)) {

								/*
								 * Bug 2185 xplot() not what is expected for large and small numbers
								 * 1. Everything was DV_FLOAT because of a missing break;
								 * 2. Plotting everything as DV_FLOAT %g is fairly limiting.
								 *    Checking for the various int's and float's and
								 *    printing accordingly gives more visually appealing results.
								 *
								 */
								// fprintf(fp, "%g\t %g\n", x[k], y[k]);  // <-- drd originally, no
								// checking

								if (V_FORMAT(v) <= DV_INT64) {
									fprintf(fp, "%ld\t %ld\n", (i64)x[k], (i64)y[k]);
								} else {
									fprintf(fp, "%f\t %f\n", x[k], y[k]);
								}
							}
						}

						if (Sep) {
							fclose(fp);
							if (count++) strcat(buf, ",");
							sprintf(buf + strlen(buf), "'%s'", fname);
							if (V_NAME(v)) sprintf(buf + strlen(buf), "title '%s'", V_NAME(v));
							free(fname);
						} else
							fprintf(fp, "\n");
					}
				}
				free(x);
				free(y);
				if (!(Sep)) {
					fclose(fp);
					if (count++) {
						strcat(buf, ",");
					}
					sprintf(buf + strlen(buf), "'%s'", fname);
					if (V_NAME(v)) {
						sprintf(buf + strlen(buf), "title '%s'", V_NAME(v));
					}
					free(fname);
				}
			}
		}
		if (strlen(buf) > 5) {
			send_to_plot(buf);
		}
	}
	free(av);
	return (NULL);
}

Var* ff_xplot2(vfuncptr func, Var* arg)
{
	/*      Modified Plot; Created:
	 **   Mon Jul 31 10:21:29 MST 2000
	 **   --Ben
	 */

	Var *s, *v;
	FILE* fp;
	char* fname;

	char buf[5120] = {0}; /* added factor of 10 */
	int count      = 0;

	Var* Xaxis = NULL;

	int Mode[3];

	int Ord[3];
	int XOrd[3];

	int CE[3];

	int xFlag  = 0;
	char* Axis = NULL;
	int obj_index;
	float *x, *y;
	int i, j, k;
	int idx;
	int Sep = 0;

	char axs;

	int ac;
	Var** av;
	float ignore;
	int iflag = 0;

	make_args(&ac, &av, func, arg); /*chop up the args into an array */
	/* make_args puts func->name into av[0] and ac == 2 */

	for (i = 1; i <= ac; i++) {
		if (av[i] == NULL) continue;

		if (V_TYPE(av[i]) == ID_KEYWORD) {
			if (!(strcmp(av[i]->name, "Xaxis"))) {
				Xaxis = eval(V_KEYVAL(av[i]));
				if (Xaxis == NULL) {
					parse_error("Variable not found: Xaxis=...");
					free(av);
					return (NULL);
				}
			} else if (!(strcmp(av[i]->name, "axis"))) {
				v = V_KEYVAL(av[i]);
				if (V_TYPE(v) == ID_STRING)
					Axis = V_STRING(v);
				else
					Axis = V_NAME(v);

			} else if (!(strcmp(av[i]->name, "separate"))) {
				Sep = 1;
			} else if (!(strcmp(av[i]->name, "ignore"))) {
				if (V_KEYVAL(av[i]) != NULL) {
					ignore = extract_float(V_KEYVAL(av[i]), 0);
					iflag  = 1;
				} else {
					parse_error("Bad value for ignore");
					free(av);
					return (NULL);
				}
			} else {
				parse_error("Illegal keyword %s", av[i]->name);
				free(av);
				return (NULL);
			}
			av[i] = NULL;
		}
	}

	if (Xaxis != NULL) xFlag = 1;

	if (Axis == NULL) {
		for (idx = 1; idx <= ac; idx++) {
			if (av[idx] == NULL) continue;

			s = av[idx];

			if (V_TYPE(s) == ID_UNK) {
				if ((s = eval(av[idx])) == NULL) {
					parse_error("Unknown Variable\n");
					free(av);
					return (NULL);
				}
			}
			if (V_TYPE(s) != ID_VAL) continue;

			Find_Axis(&axs, s);
			Axis = strdup(&axs);
			break;
		}
		if (Axis == NULL) {
			parse_error("xplot: No variables.\n");
			return (NULL);
		}
	}
	switch (*Axis) {
	case 'X':
	case 'x':
		Mode[0] = 0;
		Mode[1] = 1;
		Mode[2] = 2;
		break;
	case 'Y':
	case 'y':
		Mode[0] = 1;
		Mode[1] = 0;
		Mode[2] = 2;
		break;
	case 'Z':
	case 'z':
		Mode[0] = 2;
		Mode[1] = 0;
		Mode[2] = 1;
		break;
	default:
		Mode[0] = 1;
		Mode[1] = 0;
		Mode[2] = 2;
		break;
	}

	if (Xaxis != NULL) {
		XOrd[0] = GetSamples(V_SIZE(Xaxis), V_ORG(Xaxis));
		XOrd[1] = GetLines(V_SIZE(Xaxis), V_ORG(Xaxis));
		XOrd[2] = GetBands(V_SIZE(Xaxis), V_ORG(Xaxis));
	}
	if (ac == 2 && V_TYPE(av[1]) == ID_STRING) {
		strcpy(buf, V_STRING(av[1]));
	} else {
		strcpy(buf, "plot ");
		for (idx = 1; idx <= ac; idx++) {
			s = av[idx];

			if (s == NULL) continue;

			if (V_TYPE(s) == ID_UNK) {
				if ((s = eval(av[idx])) == NULL) {
					parse_error("Unknown Variable\n");
					free(av);
					return (NULL);
				}
			}
			switch (V_TYPE(s)) {

			case ID_STRING:
				strcat(buf, V_STRING(s));
				count++;
				break;
			case ID_VAL:
				if (!(Sep)) {
					fname = make_temp_file_path();
					if (fname == NULL || (fp = fopen(fname, "w")) == NULL) {
						parse_error("%s: unable to open temp file", func->name);
						if (fname) free(fname);
						return (NULL);
					}
				}
				if ((v = eval(s)) == NULL) v = s;

				Ord[0] = GetSamples(V_SIZE(v), V_ORG(v));
				Ord[1] = GetLines(V_SIZE(v), V_ORG(v));
				Ord[2] = GetBands(V_SIZE(v), V_ORG(v));

				if (xFlag) {

					if (XOrd[Mode[0]] != Ord[Mode[0]]) {
						parse_error("Given X-Axis doesn't agree with given data set");
						free(av);
						return (NULL);
					} else if ((XOrd[1] != 1 && XOrd[2] != 1) &&
					           (XOrd[1] != Ord[Mode[1]] && XOrd[2] != Ord[Mode[2]])) {
						parse_error("Given X-Axis doesn't agree with given data set");
						free(av);
						return (NULL);
					}
				}
				// It's unclear why these are arrays.
				// It doesn't appear that the values are used
				// outside of the for loop.
				x = calloc(Ord[Mode[0]], sizeof(float));
				y = calloc(Ord[Mode[0]], sizeof(float));

				for (i = 0; i < Ord[Mode[2]]; i++) {
					for (j = 0; j < Ord[Mode[1]]; j++) {

						if (Sep) {
							fname = make_temp_file_path();
							if (fname == NULL || (fp = fopen(fname, "w")) == NULL) {
								parse_error("%s: unable to open temp file", func->name);
								if (fname) {
									free(fname);
								}
								// Lots of memory leaks here
								return (NULL);
							}
						}
						for (k = 0; k < Ord[Mode[0]]; k++) {
							CE[Mode[2]] = i;
							CE[Mode[1]] = j;
							CE[Mode[0]] = k;
							obj_index   = cpos(CE[0], CE[1], CE[2], v);

							y[k] = extract_float(v, obj_index);

							if (xFlag) {
								x[k] = extract_float(Xaxis, rpos(obj_index, v, Xaxis));
							} else {
								x[k] = (float)k;
							}
							if (iflag && x[k] != ignore && y[k] != ignore) {
								fprintf(fp, "%g\t %g\n", x[k], y[k]);
							}
						}

						if (Sep) {
							fclose(fp);
							if (count++) strcat(buf, ",");
							sprintf(buf + strlen(buf), "'%s'", fname);
							if (V_NAME(v)) sprintf(buf + strlen(buf), "title '%s'", V_NAME(v));
							free(fname);
						} else
							fprintf(fp, "\n");
					}
				}
				free(x);
				free(y);
				if (!(Sep)) {
					fclose(fp);
					if (count++) strcat(buf, ",");
					sprintf(buf + strlen(buf), "'%s'", fname);
					if (V_NAME(v)) sprintf(buf + strlen(buf), "title '%s'", V_NAME(v));
					free(fname);
				}
			}
		}
	}
	send_to_plot(buf);
	free(av);
	return (NULL);
}

/*
xplot2(
        data = { a, b, c }
        Xaxis = { xa, xb, xc }
        style = { sa, sb, sc }

        ignore = { ia, ib, ic }
        separate = { 0, 0, 0 }

*/
