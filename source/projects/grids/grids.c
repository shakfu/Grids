//
//     Grids for Max for Live.
//
// Port of Mutable Instruments Grids for Max4Live
//
// Author: Henri DAVID
// https://github.com/hdavids/Grids
//
// Based on code of Olivier Gillet (ol.gillet@gmail.com)
// https://github.com/pichenettes/eurorack/tree/master/grids
//
// forked/updated to Max 8 idioms by S. Alireza 
// https://github.com/shakfu/grids-for-max
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//


#include "ext.h"
#include "ext_obex.h"
#include "resources.h"
#include <math.h>

// The standard random() function is not standard on Windows.
// We need to do this to setup the rand_s() function.
#ifdef WIN_VERSION
#define _CRT_RAND_S
#endif

typedef struct _grids
{
	t_object ob;
	t_atom   val;
	t_symbol *name;

    // constants
	t_uint8 num_parts;
	t_uint8 steps_per_pattern;

	//parameters
	t_uint8 mode;
	t_uint8 map_x;
	t_uint8 map_y;
	t_uint8 randomness;
	t_uint8 euclidean_length[3];
	t_uint8 density[3];
	t_uint8 euclidean_step[3];

	//running vars
	t_uint8 part_perturbation[3];
	t_uint8 step;

	//output
	t_uint8 velocities[3];
	t_uint8 state;

	//outlets
	void *outlet_kick_gate;
	void *outlet_snare_gate;
	void *outlet_hihat_gate;
	void *outlet_kick_accent_gate;
	void *outlet_snare_accent_gate;
	void *outlet_hihat_accent_gate;

} t_grids;

//static t_class *s_grids_class = NULL;
void *grids_class;

//max ojbect creation
void *grids_new(t_symbol *s, long argc, t_atom *argv);
void grids_free(t_grids *g);
void grids_assist(t_grids *g, void *b, long m, long a, char *s);

//max inlets
void grids_in_kick_density(t_grids *g, long n);
void grids_in_snare_density(t_grids *g, long n);
void grids_in_hihat_density(t_grids *g, long n);
void grids_in_map_x(t_grids *g, long n);
void grids_in_map_y(t_grids *g, long n);
void grids_in_randomness(t_grids *g, long n);
void grids_in_kick_euclidian_length(t_grids *g, long n);
void grids_in_snare_euclidian_length(t_grids *g, long n);
void grids_in_hihat_euclidian_length(t_grids *g, long n);
void grids_in_mode_and_clock(t_grids *g, long n);

//grids
void grids_run(t_grids *g, long playHead);
void grids_reset(t_grids *g);
void grids_evaluate(t_grids *g);
void grids_evaluate_drums(t_grids *g);
t_uint8 grids_read_drum_map(t_grids *g, t_uint8 instrument);
void grids_evaluate_euclidean(t_grids *g);
void grids_output(t_grids *g);


void ext_main(void *r)
{
	t_class *c;

	//create grids object
	c = class_new("grids", (method)grids_new, (method)grids_free, 
				           (long)sizeof(t_grids), 0L, A_GIMME, 0);

	//inlet definition
	class_addmethod(c, (method)grids_in_kick_density, "int", A_LONG, 0);
	class_addmethod(c, (method)grids_in_snare_density, "in1", A_LONG, 0);
	class_addmethod(c, (method)grids_in_hihat_density, "in2", A_LONG, 0);
	class_addmethod(c, (method)grids_in_map_x, "in3", A_LONG, 0);
	class_addmethod(c, (method)grids_in_map_y, "in4", A_LONG, 0);
	class_addmethod(c, (method)grids_in_randomness, "in5", A_LONG, 0);
	class_addmethod(c, (method)grids_in_kick_euclidian_length, "in6", A_LONG, 0);
	class_addmethod(c, (method)grids_in_snare_euclidian_length, "in7", A_LONG, 0);
	class_addmethod(c, (method)grids_in_hihat_euclidian_length, "in8", A_LONG, 0);
	class_addmethod(c, (method)grids_in_mode_and_clock, "in9", A_LONG, 0);

	//inlet tooltips
	class_addmethod(c, (method)grids_assist, "assist", A_CANT, 0);

	CLASS_ATTR_SYM(c, "name", 0, t_grids, name);

	class_register(CLASS_BOX, c);
	grids_class = c;
}


void *grids_new(t_symbol *s, long argc, t_atom *argv)
{

	t_grids *g = NULL;

	if ((g = (t_grids *)object_alloc(grids_class))) {

		//inlets
		for (t_uint8 i = 9; i > 0; i--)
			intin(g, i);


		//outlets
		g->outlet_hihat_accent_gate = intout((t_object *)g);
		g->outlet_snare_accent_gate = intout((t_object *)g);
		g->outlet_kick_accent_gate = intout((t_object *)g);
		g->outlet_hihat_gate = intout((t_object *)g);
		g->outlet_snare_gate = intout((t_object *)g);
		g->outlet_kick_gate = intout((t_object *)g);

		//configuration
		g->num_parts = 3;
		g->steps_per_pattern = 32;

		//parameters
		g->map_x = 64;
		g->map_y = 64;
		g->randomness = 10;
		g->mode = 0;
		g->euclidean_length[0] = 5;
		g->euclidean_length[1] = 7;
		g->euclidean_length[2] = 11;
		g->density[0] = 32;
		g->density[1] = 32;
		g->density[2] = 32;

		//runing vars
		g->part_perturbation[0] = 0;
		g->part_perturbation[1] = 0;
		g->part_perturbation[2] = 0;
		g->euclidean_step[0] = 0;
		g->euclidean_step[1] = 0;
		g->euclidean_step[2] = 0;
		g->step = 0;

		//output
		g->state = 0;
		g->velocities[0] = 0;
		g->velocities[1] = 0;
		g->velocities[2] = 0;

	}

	return g;
}



void grids_assist(t_grids *g, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) {	
		// Inlets
		switch (a) {
		case 0: sprintf(s, "kick density"); break;
		case 1: sprintf(s, "snare density"); break;
		case 2: sprintf(s, "hihat density"); break;
		case 3: sprintf(s, "map X"); break;
		case 4: sprintf(s, "map Y"); break;
		case 5: sprintf(s, "randomness"); break;
		case 6: sprintf(s, "kick euclidian length"); break;
		case 7: sprintf(s, "snare euclidian length"); break;
		case 8: sprintf(s, "hihat euclidian length"); break;
		case 9: sprintf(s, "mode/clock (-1:drums, -2:euclidian, n>=0:clock)"); break;
		}
	} else {		
		// Outlets
		switch (a) {
		case 0: sprintf(s, "kick gate"); break;
		case 1: sprintf(s, "snare gate"); break;
		case 2: sprintf(s, "hihat gate"); break;
		case 3: sprintf(s, "kick accent gate"); break;
		case 4: sprintf(s, "snare accent gate"); break;
		case 5: sprintf(s, "hihat accent gate"); break;
		}
	}
}



void grids_free(t_grids *g)
{
}

// Max Inlets

void grids_in_kick_density(t_grids *g, long n)
{
	if (n >= 0 && n <= 127) g->density[0] = (t_uint8)n;
}

void grids_in_snare_density(t_grids *g, long n)
{
	if (n >= 0 && n <= 127) g->density[1] = (t_uint8)n;
}

void grids_in_hihat_density(t_grids *g, long n)
{
	if (n >= 0 && n <= 127) g->density[2] = (t_uint8)n;
}

void grids_in_map_x(t_grids *g, long n)
{
	if (n >= 0 && n <= 127) g->map_x = (t_uint8)(n % 256);
}
void grids_in_map_y(t_grids *g, long n)
{
	if (n >= 0 && n <= 127) g->map_y = (t_uint8)n;
}

void grids_in_randomness(t_grids *g, long n)
{
	if (n >= 0 && n <= 127) g->randomness = (t_uint8)n;
}

void grids_in_kick_euclidian_length(t_grids *g, long n)
{
	if (n > 0 && n <= 32) g->euclidean_length[0] = (t_uint8)(n);
}

void grids_in_snare_euclidian_length(t_grids *g, long n)
{
	if (n > 0 && n <= 32) g->euclidean_length[1] = (t_uint8)(n);
}

void grids_in_hihat_euclidian_length(t_grids *g, long n)
{
	if (n > 0 && n <= 32) g->euclidean_length[2] = (t_uint8)(n);
}


// Grids

void grids_in_mode_and_clock(t_grids *g, long n)
{
	if (n >= 0) {
		grids_run(g, n);
	} else {
		if (n == -1) {
			g->mode = 0;
		}if (n == -2) {
			g->mode = 1;
		}if (n == -3) {
			grids_reset(g);
		}
	}
}


void grids_run(t_grids *g, long playHead) {
	g->step = playHead % 32;
	g->state = 0;
	if (g->mode == 1) {
		grids_evaluate_euclidean(g);
	}
	else {
		grids_evaluate_drums(g);
	}
	grids_output(g);

	//increment euclidian clock.
	for (int i = 0; i < g->num_parts; i++)
		g->euclidean_step[i] = (g->euclidean_step[i] + 1) % g->euclidean_length[i];
	
}

void grids_reset(t_grids *g) {
	g->euclidean_step[0] = 0;
	g->euclidean_step[1] = 0;
	g->euclidean_step[2] = 0;
	g->step = 0;
	g->state = 0;
	//object_post((t_object *)g, "reset");
}


void grids_output(t_grids *g) {
	if ((g->state & 1) > 0)
		outlet_int(g->outlet_kick_gate, g->velocities[0]);
	if ((g->state & 2) > 0)
		outlet_int(g->outlet_snare_gate, g->velocities[1]);
	if ((g->state & 4) > 0)
		outlet_int(g->outlet_hihat_gate, g->velocities[2]);

	if ((g->state & 8) > 0)
		outlet_int(g->outlet_kick_accent_gate, g->velocities[0]);
	if ((g->state & 16) > 0) 
		outlet_int(g->outlet_snare_accent_gate, g->velocities[1]);
	if ((g->state & 32) > 0)
		outlet_int(g->outlet_hihat_accent_gate, g->velocities[2]);
	
}

void grids_evaluate_drums(t_grids *g) {
	// At the beginning of a pattern, decide on perturbation levels
	if (g->step == 0) {
		for (int i = 0; i < g->num_parts; ++i) {
			t_uint8 randomness = g->randomness >> 2;
#ifdef WIN_VERSION
			unsigned int rand;
			rand_s(&rand);
#else
			t_uint8 rand = random();
#endif
			t_uint8 rand2 = (t_uint8)(rand%256);
			g->part_perturbation[i] = (rand2*randomness) >> 8;
		}
	}

	t_uint8 instrument_mask = 1;
	t_uint8 accent_bits = 0;
	for (int i = 0; i < g->num_parts; ++i) {
		t_uint8 level = grids_read_drum_map(g, i);
		if (level < 255 - g->part_perturbation[i]) {
			level += g->part_perturbation[i];
		} else {
			// The sequencer from Anushri uses a weird clipping rule here. Comment
			// this line to reproduce its behavior.
			level = 255;
		}
		t_uint8 threshold = 255 - g->density[i] * 2;
		if (level > threshold) {
			if (level > 192) {
				accent_bits |= instrument_mask;
			}
			g->velocities[i] = level / 2;
			g->state |= instrument_mask;
		}
		instrument_mask <<= 1;
	}
	g->state |= accent_bits << 3;
}


t_uint8 grids_read_drum_map(t_grids *g, t_uint8 instrument) {

	t_uint8 x = g->map_x;
	t_uint8 y = g->map_y;
	t_uint8 step = g->step;

	int i = (int)floor(x*3.0 / 127);
	int j = (int)floor(y*3.0 / 127);
	t_uint8* a_map = drum_map[i][j];
	t_uint8* b_map = drum_map[i + 1][j];
	t_uint8* c_map = drum_map[i][j + 1];
	t_uint8* d_map = drum_map[i + 1][j + 1];

	int offset = (instrument * g->steps_per_pattern) + step;
	t_uint8 a = a_map[offset];
	t_uint8 b = b_map[offset];
	t_uint8 c = c_map[offset];
	t_uint8 d = d_map[offset];
	t_uint8 maxValue = 127;
	t_uint8 r = ( 
					  ( a * x	+ b * (maxValue - x) ) * y 
					+ ( c * x	+ d * (maxValue - x) ) * ( maxValue - y ) 
				) / maxValue / maxValue;
	return r;
}


void grids_evaluate_euclidean(t_grids *g) {
	t_uint8 instrument_mask = 1;
	t_uint8 reset_bits = 0;
	// Refresh only on sixteenth notes.
	if (!(g->step & 1)) {
		for (int i = 0; i < g->num_parts; ++i) {
			g->velocities[i] = 100;
			t_uint8 density = g->density[i] >> 2;
			t_uint16 address = (g->euclidean_length[i] - 1) * 32 + density;
			t_uint32 step_mask = 1L << (t_uint32)g->euclidean_step[i];
			t_uint32 pattern_bits = address < 1024 ? grids_res_euclidean[address] : 0;
			if (pattern_bits & step_mask) 
				g->state |= instrument_mask;
			if (g->euclidean_step[i] == 0)
				reset_bits |= instrument_mask;
			instrument_mask <<= 1;
		}
	}
	g->state |= reset_bits << 3;
}

