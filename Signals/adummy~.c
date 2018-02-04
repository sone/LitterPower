#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "ext_common.h"

/*
	a dummy object for signel level externals
*/


typedef struct {
	t_pxobject x_obj;
	double	sr;
	double	myfloat;
	int		myint;

} t_myObj;


static t_class *myObj_class;

void myObj_int(t_myObj *x, long input);
void myObj_float(t_myObj *x, double input);

// DSP methods
void myObj_dsp(t_myObj *x, t_signal **sp, short *count);
t_int *myObj_perform(t_int *w);

void myObj_dsp64(t_myObj *x, t_object *dsp64, short *count, double samplerate, 
				 long maxvectorsize, long flags);
void myObj_perform64(t_myObj *x, t_object *dsp64, double **ins, long numins, 
					 double **outs, long numouts, long sampleframes, long flags, void *userparam);
//

void *myObj_new( t_symbol *s, long argc, t_atom *argv);
void myObj_assist(t_myObj *x, void *b, long m, long a, char *s);


int C74_EXPORT main(void) {
	t_class *c;
	
	c = class_new("adummy~", (method)myObj_new, (method)dsp_free, (short)sizeof(t_myObj), 0L,
				  A_GIMME, 0L);
	class_addmethod(c, (method)myObj_dsp, "dsp", A_CANT, 0);
	class_addmethod(c, (method)myObj_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)myObj_float, "float", A_FLOAT, 0);	
	class_addmethod(c, (method)myObj_int, "int", A_LONG, 0);
	class_addmethod(c, (method)myObj_assist, "assist", A_CANT,0);
	class_dspinit(c);
	class_register(CLASS_BOX, c);
	myObj_class = c;
	
	
	// attributes ==== noch nicht ganz fertig
	/*
	CLASS_ATTR_LONG(c,"interp", 0, t_myObj, interpol);
	CLASS_ATTR_DEFAULT_SAVE(c, "interp", 0, "1");		// default geht nicht.
	CLASS_ATTR_SAVE(c, "interp", 0);
	CLASS_ATTR_MIN(c, "interp", 0, "0");
	CLASS_ATTR_MAX(c, "interp", 0, "1");
	*/
	
	post("adummy~ a new dummy object");
	
	return 0;
}



void myObj_int(t_myObj *x, long input) {
	x->myint = input;
	object_post((t_object *)x, "myint: %d", x->myint); 
}

void myObj_float(t_myObj *x, double input) {
	x->myfloat = input;
	object_post((t_object *)x, "myfloat: %f", x->myfloat); 
}


// 32-bit dsp method
void myObj_dsp(t_myObj *x, t_signal **sp, short *count) {

	dsp_add(myObj_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
	
	x->sr = sp[0]->s_sr;
	if(x->sr<=0) x->sr = 44100.0;
	
	object_post((t_object *)x, "sr: %f", x->sr);
}


//64-bit dsp method
void myObj_dsp64(t_myObj *x, t_object *dsp64, short *count, double samplerate, 
				 long maxvectorsize, long flags) {
	object_method(dsp64, gensym("dsp_add64"), x, myObj_perform64, 0, NULL);
	
	x->sr = samplerate;
	if(x->sr<=0) x->sr = 44100.0;

}



t_int *myObj_perform(t_int *w) {
	
	t_myObj *x = (t_myObj*)(w[1]);
	float *in = (float *)(w[2]);
	float *out = (float *)(w[3]);	
	int vs = (int)(w[4]);		
	
	
	if (x->x_obj.z_disabled)
		goto out;
	if(x->myfloat == 0)
		goto zero;

	while (vs--) {
		*out++ = (*in++) * x->myfloat;
	}
	
out:
	return w+5;	
	
zero:
	while (vs--)  
		*out++ = 0.f;
	return w+5;	
}




// 64 bit signal input version
void myObj_perform64(t_myObj *x, t_object *dsp64, double **ins, long numins, 
					   double **outs, long numouts, long sampleframes, long flags, void *userparam){
	
	t_double *in = ins[0];
	t_double *out = outs[0];	
	int vs = sampleframes;		

	
	if (x->x_obj.z_disabled)
		return;
	if(x->myfloat == 0)
		goto zero;

	while(vs--) {
		*out++ = (*in++) * x->myfloat;
	}

	return;
	
zero:
	while (vs--)  
		*out++ = 0.;
}





void *myObj_new(t_symbol *s, long argc, t_atom *argv)
{
	t_myObj *x = object_alloc(myObj_class);
	if(x) {
		dsp_setup((t_pxobject*)x, 1);			// one signal inlet
		//outlet_new((t_object *)x, "signal"); 
		outlet_new(x, "signal"); 
		
		x->sr = sys_getsr();
		if(x->sr <= 0)
			x->sr = 44100.;
		
		x->myint = 0;
		x->myfloat = 1.;
		
		//attr_args_process(x, argc, argv);			// process attributes
	}
	
	else {
		object_free(x);
		x = NULL;
	}
		
	
	return x;
}


void myObj_assist(t_myObj *x, void *b, long m, long a, char *s) {
	if (m == ASSIST_INLET) {
		switch(a) {
			case 0: sprintf (s,"(signal/float/int) ..."); break;
		}
	}
	else {
		switch(a) {
			case 0: sprintf (s,"(signal) signal out"); break;
			//case 1: sprintf(s, "(signal) s & h random values out"); break;
		}
		
	}
}
