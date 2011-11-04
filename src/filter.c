
/*
 * Copyright (c) 2011, Álvaro Vilanova Vidal
 * Copyright (c) 2011, Gustavo Lanza Duarte
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * The names of its contributors may be not used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include "stb_reader.h"
#include "stb_writer.h"

#define BUFFER_SIZE     128
#define STR_BUFFER_SIZE 512
#define PRG_BUFFER_SIZE 1024 * 4

/*
 * Obtiene todas las plataformas OpenCL disponibles en el sistema.
 * > ps      : (gc) Array con los IDs de las plataformas.
 * > ps_size : Tamaño del array (número de plataformas).
 */
cl_int getAllPlatforms (cl_platform_id **ps, cl_uint *ps_size) {
	cl_int ret;
	if (!(ret = clGetPlatformIDs(0, NULL, ps_size))) {
		*ps = (cl_platform_id*) malloc(sizeof(cl_platform_id*) * (*ps_size));
		ret = clGetPlatformIDs(*ps_size, *ps, NULL);
	}
	return ret;
}

/*
 * Obtiene todos los dispositivos de una plataforma.
 * < p       : Plataforma de la que obtener los dispositivos.
 * > ds      : (gc) Array con los IDs de los dispositivos. 
 * > ds_size : Tamaño del array (número de dispositivos).
 */
cl_int getAllDevices (cl_platform_id p, cl_device_id **ds, cl_uint *ds_size) {
	cl_int ret;
	if (!(ret = clGetDeviceIDs(p, CL_DEVICE_TYPE_ALL, 0, NULL, ds_size))) {
		*ds = (cl_device_id*) malloc(sizeof(cl_device_id*) * (*ds_size));
		ret = clGetDeviceIDs(p, CL_DEVICE_TYPE_ALL, *ds_size, *ds, NULL);
	}
	return ret;
}

/*
 * Muestra por pantalla información sobre todas las plataformas y dispositivos
 * compatibles con OpenCL en el sistema.
 */
void showClInfo () {
	cl_platform_id *ps;
	cl_device_id *ds = NULL;
	cl_uint ps_size, ds_size, i, j;
	char name[STR_BUFFER_SIZE], vendor[STR_BUFFER_SIZE];

	getAllPlatforms(&ps, &ps_size);
	for (i = 0; i < ps_size; i++) {
		clGetPlatformInfo(ps[i], CL_PLATFORM_NAME, STR_BUFFER_SIZE,
			(void*)name, NULL);
		clGetPlatformInfo(ps[i], CL_PLATFORM_VENDOR, STR_BUFFER_SIZE,
			(void*)vendor, NULL);
		printf("Platform #%d - %s (%s)\n", i, name, vendor);
		getAllDevices(ps[i], &ds, &ds_size);
		for (j = 0; j < ds_size; j++) {
			clGetDeviceInfo(ds[j], CL_DEVICE_NAME, STR_BUFFER_SIZE,
				(void*)name, NULL);
			clGetDeviceInfo(ds[j], CL_DEVICE_VENDOR, STR_BUFFER_SIZE,
				(void*)vendor, NULL);
			printf("\tDevice #%d - %s (%s)\n", j, name, vendor);
		}
		free(ds);
	}
	free(ps);
}

/*
 * Obtiene un contexto a partir de los parámetros introducidos por la línea de
 * comandos.
 * < arg : Cadena que contiene el parámetro que selecciona los dispositvos. Debe
 *         ser de forma: List = p_id-d_id[[;, ]List].
 * > sel_ds : Array con los dispitivos seleccionados.
 */
cl_context createContextFromArgs (char *arg, cl_device_id *sel_ds,
								  cl_uint *sel_ds_size) {
	cl_uint is[BUFFER_SIZE], is_size = 0, ps_size, ds_sizes[BUFFER_SIZE], i,
			props_size = 0;
	char *str, tmp[STR_BUFFER_SIZE];
	cl_platform_id *ps, p;
	cl_device_id *ds[BUFFER_SIZE], d;
	cl_context_properties props[BUFFER_SIZE];

	*sel_ds_size = 0;
	if (arg == NULL || arg[0] == '\0')
		return NULL;

	/* Obtenemos todos indices a partir de la cadena de entrada */
	str = strtok(strcpy(tmp, arg), ",; ");
	do {
		if (sscanf(str, "%u-%u", &is[is_size], &is[is_size + 1]) >= 2)
			is_size += 2;
	} while ((str = strtok(NULL, ",; ")) != NULL);
	if (is_size == 0)
		return NULL;

	/* Obtenemos todas las plataformas y dispositivos del sistema */
	getAllPlatforms(&ps, &ps_size);
	for (i = 0; i < ps_size; i++) {
		getAllDevices(ps[i], &ds[i], &ds_sizes[i]);
	}

	/* Obtenemos los dispositivos seleccionados a partir de los indices */
	for (i = 0; i < is_size; i += 2) {
		if (is[i] >= ps_size || is[i + 1] >= ds_sizes[is[i]]) {
			fprintf(stderr, "ERROR: Device %d-%d does not exist\n",
					is[i], is[i + 1]);
			return NULL;
		}
		else {
			p = ps[is[i]];
			d = ds[is[i]][is[i + 1]];
			props[props_size++] = CL_CONTEXT_PLATFORM;
			props[props_size++] = (cl_context_properties)p;
			sel_ds[(*sel_ds_size)++] = d;
		}
	}
	props[props_size] = (cl_context_properties)NULL;

	/* Liberamos recursos */
	free(ps);
	for (i = 0; i < ps_size; i++)
		free(ds[i]);
	
	/* Creamos y devolvemos el contexto */
	return clCreateContext(props, *sel_ds_size, sel_ds, NULL, NULL, NULL);
}

/*
 * Función que a partir de la ruta del código fuente y los dispositivos 
 * seleccionados crea ruta del código compilado.
 * > arg : Parámetro con los dispositivos seleccionados.
 * > src : Ruta del código fuente.
 * < bin : Ruta del archivo binario.
 */
void sourceToBinPath (char *arg, char *src, char *bin) {
	char code[STR_BUFFER_SIZE], *ptr;

	for(ptr = code; *arg != '\0'; arg++)
		if (*arg >= '0' && *arg <= '9')
			*(ptr++) = *arg;
	*ptr = *bin = '\0';

	strcat(strcat(strncat(bin, src, strcspn(src, ".")), code), ".bin");
}

/*
 * Muestra la información de registro de la compilación de un programa. Se
 * utiliza cuando se encuentrar errores en el programa CL.
 * > ds      : Array de dispositivos para los que fue compilado el programa.
 * > ds_size : Tamaño del array anterior.
 * > prg     : Programa del que se quiere mostrar la información.
 */
void showProgramBuildLog (cl_device_id *ds, cl_uint ds_size, cl_program prg) {
	size_t i, log_size;
	char name[STR_BUFFER_SIZE], *log;
	
	for (i = 0; i < ds_size; i++) {
		clGetDeviceInfo(ds[i], CL_DEVICE_NAME, STR_BUFFER_SIZE,
		                (void*)name, NULL);	
		printf("LOG BUILD FROM %s\n", name);
		clGetProgramBuildInfo(prg, ds[i], CL_PROGRAM_BUILD_LOG, 0, NULL,
		                      &log_size);
		log = malloc(log_size * sizeof(char));
		clGetProgramBuildInfo(prg, ds[i], CL_PROGRAM_BUILD_LOG, log_size, log,
		                      NULL);
		printf("%s\n", log);
		free(log);
	}
}

/*
 * Carga un fichero binario con los programas compilados para los dispositivos
 * seleccionados.
 * > ctx     : Contexto del programa.
 * > ds      : Array con los dispositivos seleccionados.
 * > ds_size : Tamaño del array anterior.
 * > bin     : Ruta al fichero binario a cargar.
 */
cl_program loadBinaryProgram (cl_context ctx, cl_device_id *ds, cl_uint ds_size,
                              char *bin) {
	FILE *f;
	cl_program prg;
	unsigned char **bins;
	size_t num, *ss, i;
	
	if ((f = fopen(bin, "rb")) != NULL) {
		fread(&num, sizeof(size_t), 1, f);
		ss = malloc(sizeof(size_t) * num);
		fread(ss, sizeof(size_t), num, f);
		bins = malloc(sizeof(unsigned char*) * num);
		for (i = 0; i < num; i++) {
			bins[i] = malloc(ss[i]);
			fread(bins[i], 1, ss[i], f);
		}

		prg = clCreateProgramWithBinary(ctx, ds_size, ds, ss,
		 	(const unsigned char **)bins, NULL, NULL);

		for (i = 0; i < num; i++) {
			free(bins[i]);
		}
		free(bins);
		free(ss);
		fclose(f);
		return prg;
	}
	
	return NULL;
}

/*
 * Guarda un fichero binario con los programas compilados para los dispositivos
 * seleccionados.
 * > prg     : Programa a guardar.
 * > ds      : Array con los dispositivos seleccionados.
 * > ds_size : Tamaño del array anterior.
 * > bin     : Ruta al fichero binario a cargar.
 */
void saveBinaryProgram (cl_program prg, cl_device_id *ds, cl_uint ds_size,
                        char *bin) {
	FILE *f;
	unsigned char **bins;
	size_t size, num, *ss, i;
	
	if ((f = fopen(bin, "wb")) == NULL)
		return;

	clGetProgramInfo(prg, CL_PROGRAM_BINARY_SIZES, 0, NULL, &size);
	num = size / sizeof(size_t);
	ss = malloc(size);
	clGetProgramInfo(prg, CL_PROGRAM_BINARY_SIZES, size, ss, NULL);
	fwrite(&num, sizeof(size_t), 1, f);
	fwrite(ss, sizeof(size_t), num, f);

	bins = malloc(num * sizeof(unsigned char*));
	for (i = 0; i < num; i++)
		bins[i] = malloc(ss[i]);
	clGetProgramInfo(prg, CL_PROGRAM_BINARIES,
					 num * sizeof(unsigned char*), bins, NULL);
	for (i = 0; i < num; i++)
		fwrite(&bins[i], 1, ss[i], f);
	
	for (i = 0; i < num; i++)
		free(bins[i]);
	free(bins);
	free(ss);
	fclose(f);
}

/*
 * Carga un programa a partir de un fichero binario (ya compilado), o, si este
 * no existe, compila y construye un programa a partir de un fichero con el
 * código fuente.
 * > ctx : Contexto en el cual crear el programa.
 * > ds  : Array con los dispostivos seleccionados.
 * > src : Ruta del código fuente.
 * > bin : Ruta del fichero binario.
 */
cl_program loadProgramFromArgs (cl_context ctx, cl_device_id *ds, 
								cl_uint ds_size, char *src, char *bin) {
	FILE *f;
	char *data;
	cl_program prg;
	size_t size;

	if ((f = fopen(bin, "rb")) != NULL)
		return loadBinaryProgram(ctx, ds, ds_size, bin);

	if ((f = fopen(src, "r")) != NULL) {
		printf("Compiling %s... ", src);
		fflush(stdout);
		data = (char*) malloc(PRG_BUFFER_SIZE * sizeof(char));
		size = fread(data, 1, PRG_BUFFER_SIZE, f);
		fclose(f);
		data[size] = '\0';
		prg = clCreateProgramWithSource(ctx, 1, (const char**)&data, NULL,
			NULL);

		if(clBuildProgram(prg, ds_size, ds, NULL, NULL, NULL) != CL_SUCCESS) {
			fprintf(stderr, "FAIL\n");
			showProgramBuildLog(ds, ds_size, prg);
			return NULL;
		}
		printf("OK\n");

		saveBinaryProgram(prg, ds, ds_size, bin);
		return prg;
	}

	return NULL;
}

int main (int argc, char *argv[]) {
	cl_context context;
	cl_device_id ds[BUFFER_SIZE];
	cl_uint ds_size;
	char bin_path[STR_BUFFER_SIZE], *src_path = "src/unlm.cl";

	if (argc < 2) {
		printf("Use: %s [platform_id-device_id]\n", argv[0]);
		showClInfo();
	}
	else {
		if((context = createContextFromArgs(argv[1], ds, &ds_size)) == NULL) {
			fprintf(stderr, "ERROR: Context can not be created\n");
			return -1;
		}

		sourceToBinPath(argv[1], src_path, bin_path);
		if(loadProgramFromArgs(context, ds, ds_size, src_path, bin_path)
		   == NULL) {
		   	fprintf(stderr, "ERROR: Program can not be loaded\n");
		   	return -1;
		}
	}

	return 0;
}

/*
		FILE *f = fopen(argv[1], "r");
		char s[STR_BUFFER_SIZE], *n;
		char pxs[300*300];
		int i = 0, h = 0, w;
		while (fscanf(f, "%s\n", s) != EOF) {
			w = 0;
			n = strtok(s, ";");
			do {
				pxs[i++] = atoi(n);
				w++;
			} while ((n =strtok(NULL, ";")) != NULL);
			h++;
		}
		fclose(f);
		printf("%dx%d\n", h, w);
		
		stbi_write_png("out.png", w, h, 1, pxs, w * sizeof(char));
*/