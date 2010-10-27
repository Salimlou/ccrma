/* xtract~ - PD library for feature extraction 
Copyright (C) 2006  Jamie Bullock

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* calculates the spectral xtract of one frame, given peak frequency and amplitude to first and second inputs respectively */

#include "ext.h"
#include "z_dsp.h"
#include <math.h>
#include <string.h>

#include "xtract/libxtract.h"

#define BLOCKSIZE 1024 /* FIX: this should be dynamic - somehow */
#define NYQUIST 22050.0f

void *xtract_tilde_class;

/* Struct for keeping track of memory allocations */
typedef struct _tracked_memory {
    char argv;
} tracked_memory;

typedef struct _xtract {
    t_pxobject  x_obj;
    void *outlet; /*Float outlet */
    t_float f;
    t_int feature;
    t_symbol *feature_name;
    t_int init_blocksize;
    t_int done_init;
    t_int feature_type;
    tracked_memory memory;
    void *argv;
} t_xtract_tilde;

static t_int *xtract_perform(t_int *w) {
    t_float *in = (t_float *)(w[1]);
    t_xtract_tilde *x = (t_xtract_tilde *)(w[2]);
    t_int N = (t_int)(w[3]);
    t_int return_code = 0;
    float result = 0.f;

    return_code = xtract[x->feature]((float *)in, N, x->argv, &result);

    if(return_code == XTRACT_FEATURE_NOT_IMPLEMENTED)
	perror("Feature not implemented");

    /* set nan, inf or -inf to 0 */
    result = (isinf(result) || isnan(result) ? 0 : result);
    
    outlet_float(x->outlet, result);
    return (w+4);
}

static t_int *xtract_perform_vector(t_int *w) {
    t_sample *in = (t_float *)(w[1]);
    t_sample *out = (t_float *)(w[2]);
    float *temp_in, *temp_out;
    t_xtract_tilde *x = (t_xtract_tilde *)(w[3]);
    t_int N = (t_int)(w[4]), n;
    t_int return_code = 0;

    if(x->feature == XTRACT_PEAK_SPECTRUM)
	N >>= 1;

    if(N != x->init_blocksize && x->done_init){
        post("xtract~ %s: Blocksize mismatch, try specifying the blocksize as a second argument", x->feature_name->s_name);
        return (w+5);
    }

    n = N;
    
    temp_in = (float *)getbytes(N * sizeof(float));
    temp_out= (float *)getbytes(N * sizeof(float));

    while(n--)
	temp_in[n] = in[n];

    n = N;

    return_code = xtract[x->feature](temp_in, N, x->argv, temp_out);
    
    while(n--)
	out[n] = temp_out[n];

    if(return_code == XTRACT_FEATURE_NOT_IMPLEMENTED)
	perror("Feature not implemented");

    freebytes(temp_in, sizeof(float) * N);
    freebytes(temp_out, sizeof(float) * N);

    return (w+5);
}

static void xtract_tilde_dsp(t_xtract_tilde *x, t_signal **sp) {

    if(x->feature_type == XTRACT_VECTOR)
        dsp_add(xtract_perform_vector, 4, 
            sp[0]->s_vec, sp[1]->s_vec, x, sp[0]->s_n);
            
    else dsp_add(xtract_perform, 3, sp[0]->s_vec, x, sp[0]->s_n);

}

static void *xtract_tilde_new(t_symbol *me, t_int argc, t_atom *argv) {
    
    t_symbol *tmp;
    t_xtract_tilde *x = (t_xtract_tilde *)newobject(xtract_tilde_class);
    xtract_mel_filter *mf;
    t_int n, N, f, F, n_args, type;
    t_float *argv_max;
    xtract_function_descriptor_t *fd;
    char *p_name, *p_desc, *author;
    int year;

    tmp = NULL;
    p_name = p_desc = author = NULL;
   
    n_args = type = x->feature = 0;

    f = F = XTRACT_FEATURES;

    N = BLOCKSIZE;
    
    x->argv = NULL;
    x->done_init = 0;
    
    if(argc)
	tmp = argv[0].a_w.w_sym; /*atom_getsymbol(argv); */
    if(argc > 1)
	N = (t_int)argv[1].a_w.w_long;

    x->init_blocksize = N;

    /* get function descriptors */
    fd = (xtract_function_descriptor_t *)xtract_make_descriptors();

    /* iterate over descriptors */
    while(f--){
	/* map creation arg to feature */
	if(tmp == gensym(fd[f].algo.name)){ 
	    x->feature = f;
	    break;
	}
    }

    /* allocate memory for feature arguments */
    n_args = fd[f].argc;
    type = fd[f].argv.type;

    if(n_args){
	for(n = 0; n < n_args; n++){
		    argv_max = &fd[f].argv.max[n]; 
		    //post("Argument %d, max: %.2f", n, *argv_max);
	}
	if(type == XTRACT_MEL_FILTER){
	    x->memory.argv = (size_t)(n_args * sizeof(xtract_mel_filter));
	    x->argv = (xtract_mel_filter *)getbytes(x->memory.argv);
	}
	else if(type == XTRACT_INT){
	    x->memory.argv = (size_t)(n_args * sizeof(t_int));
	    x->argv = (t_int *)getbytes(x->memory.argv);
	}
	else if (type == XTRACT_FLOAT){
	    x->memory.argv = (size_t)(n_args * sizeof(t_float));
	    x->argv = (t_float *)getbytes(x->memory.argv);
	}
	else
	    x->memory.argv = 0;
    }


    p_name = fd[f].algo.p_name;
    p_desc = fd[f].algo.p_desc;
    author = fd[f].algo.author;
    year = fd[f].algo.year; 

    if(argc){
	if(strcmp(p_name, ""))	
	    post("xtract~: %s", p_name );
	if(strcmp(p_desc, ""))	
	    post("xtract~: %s", p_desc );
	if(strcmp(author, "") && year)	
	    post("xtract~: %s(%d)", author, year);
    }	
    else
	post("xtract~: No arguments given");
    
    /* do init if needed */
    if(x->feature == XTRACT_MFCC){

        mf = x->argv;
        
        mf->n_filters = 20;
        
        post("xtract~: mfcc: filters = %d", 
		((xtract_mel_filter *)x->argv)->n_filters);
        mf->filters = 
            (t_float **)getbytes(mf->n_filters * sizeof(t_float *));
        for(n = 0; n < mf->n_filters; n++)
            mf->filters[n] = (float *)getbytes(N * sizeof(float));
                 
        xtract_init_mfcc(N, NYQUIST, XTRACT_EQUAL_GAIN, 80.0f,
        18000.0f, mf->n_filters, mf->filters);
    }
    else if(x->feature == XTRACT_BARK_COEFFICIENTS)
        xtract_init_bark(N, NYQUIST, x->argv);
    
    /* Initialise fft_plan if required */
    if(x->feature == XTRACT_AUTOCORRELATION_FFT ||
            x->feature == XTRACT_SPECTRUM ||
            x->feature == XTRACT_DCT){
        xtract_init_fft(N, x->feature);
        x->done_init = 1;
    }

    if(x->feature == XTRACT_AUTOCORRELATION || 
	    x->feature == XTRACT_AUTOCORRELATION_FFT || 
	    x->feature == XTRACT_MFCC || x->feature == XTRACT_AMDF || 
	    x->feature == XTRACT_ASDF|| x->feature == XTRACT_DCT || 
	    x->feature == XTRACT_BARK_COEFFICIENTS || 
	    x->feature == XTRACT_SPECTRUM || 
	    x->feature == XTRACT_PEAK_SPECTRUM || 
	    x->feature == XTRACT_HARMONIC_SPECTRUM) 
	x->feature_type = XTRACT_VECTOR;
                
    else if (x->feature == XTRACT_FLUX || x->feature == XTRACT_ATTACK_TIME || 
            x->feature == XTRACT_DECAY_TIME || x->feature == XTRACT_DELTA) 
        x->feature_type = XTRACT_DELTA;
       
    else x->feature_type = XTRACT_SCALAR;

    /* argv through right inlet */
    inlet_new((t_pxobject *)x, "list"); 

    /* DSP inlet */
    dsp_setup((t_pxobject *)x, 1);


    /* if feature is vector, create signal out */
    if(x->feature_type == XTRACT_VECTOR)  
	outlet_new((t_pxobject *)x, "signal");

    /* otherwise: float */
    else 
	x->outlet = floatout((t_pxobject *)x);

    
    /* free the function descriptors */
    xtract_free_descriptors(fd);
    
    return (void *)x;
}

static void xtract_tilde_get_args(t_xtract_tilde *x, t_symbol *selector, 
t_int argc, t_atom *argv) {
/*
    if(argc > (t_int)sizeof(x->argv) / 
                        (t_int)sizeof(t_float) || x->argv == NULL)
        post("Too many parameters to right inlet");
    else{*/

    t_float temp = 0.f;
    x->argv = getbytes(argc * sizeof(float));
    
        while(argc--){
	    temp = argv[argc].a_w.w_float;
            ((t_float *)x->argv)[argc] = temp;
	}
 /*   }*/
}

static void xtract_tilde_show_help(t_xtract_tilde *x, t_symbol *s){
    
    int i;

    i = XTRACT_FEATURES;
    xtract_function_descriptor_t *fd, *d;

    fd = (xtract_function_descriptor_t *)xtract_make_descriptors();
    post("\nxtract~: Feature List\n");
   
    while(i--){
	d = &fd[i];
	post("\t%s", d->algo.name);
    }

    xtract_free_descriptors(fd);
}

static void xtract_tilde_free(t_xtract_tilde *x) {

    if(x->argv != NULL && x->memory.argv)
        freebytes(x->argv, x->memory.argv);
}

int main(void) {

    setup((t_messlist **)&xtract_tilde_class,
    	(method)xtract_tilde_new,
    	(method)xtract_tilde_free, 
    	sizeof(t_xtract_tilde),
    	0L, 
    	A_GIMME, 0);

    addmess((method)xtract_tilde_dsp, "dsp", A_CANT,  0);
    addmess((method)xtract_tilde_get_args, "list", A_GIMME, 0);
    addmess((method)xtract_tilde_show_help, "help", A_DEFSYM, 0); 
    dsp_initclass();
    //class_setname("xtract~", "xtract~");

    return 0;

}
