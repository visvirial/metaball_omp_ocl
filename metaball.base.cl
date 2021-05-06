#include "metaball_common.h"

// Calculate and set color
void calc(__global unsigned char *data, __local const float2 *br, float2 r){
	float density = 0.0;
	for(int i=0; i<N_BALLS; i++){
		float2 rr = br[i] - r;
		density += native_recip(dot(rr, rr));
	}
	int4 col;
	if(density > THRESHOLD){
		col = (int4)(INNER_COLOR_R, INNER_COLOR_G, INNER_COLOR_B, 0);
	}else{
		float d = FACTOR * density;
		col = (int4)(d*COLOR_R, d*COLOR_G, d*COLOR_B, 0);
		if(col.x > 255) col.x = COLOR_R;
		if(col.y > 255) col.y = COLOR_G;
		if(col.z > 255) col.z = COLOR_B;
	}
	int j = 3*(r.x+r.y*WIDTH);
	data[j+2] = (unsigned char)(col.x);
	data[j+1] = (unsigned char)(col.y);
	data[j  ] = (unsigned char)(col.z);
}

// Kernel method
__kernel void metaball(__global unsigned char *data, __global const float2 *br){
	const int gi = get_global_id(0);
	if(gi >= WIDTH*HEIGHT) return;
	
	// Copy br to __local
	__local float2 br_l[N_BALLS];
	event_t ev;
	ev = async_work_group_copy(br_l, br, N_BALLS, 0);
	wait_group_events(1, &ev);
	
	// Calculate
	calc(data, br_l, (float2)(gi%WIDTH, gi/WIDTH));
}

/* vim: set filetype=c: */
