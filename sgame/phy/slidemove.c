/*
  ==============================
  Written by:
    id software :            Quake III Arena
    sOkam! :                 Opensource Defrag

  This file is part of Opensource Defrag.

  Opensource Defrag is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Opensource Defrag is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Opensource Defrag.  If not, see <http://www.gnu.org/licenses/>.
  ==============================
*/

#include "local.h"

// input:  origin, velocity, bounds, groundPlane, trace function
// output: origin, velocity, impacts, stairup boolean

//==================
// SlideMove
//   Returns qtrue if the velocity was clipped in some way
//==================
#define	MAX_CLIP_PLANES	5
// Unmodded Q3A version
qboolean	core_SlideMove( qboolean gravity ) {
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	vec3_t		clipVelocity;
	int			i, j, k;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;
	
	numbumps = 4;

	VectorCopy (pm->ps->velocity, primal_velocity);

	if ( gravity ) {
		VectorCopy( pm->ps->velocity, endVelocity );
		endVelocity[2] -= pm->ps->gravity * pml.frametime;
		pm->ps->velocity[2] = ( pm->ps->velocity[2] + endVelocity[2] ) * 0.5;
		primal_velocity[2] = endVelocity[2];
		if ( pml.groundPlane ) {
			// slide along the ground plane
			VectorReflect (pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP );
		}
	}

	time_left = pml.frametime;

	// never turn against the ground plane
	if ( pml.groundPlane ) {
		numplanes = 1;
		VectorCopy( pml.groundTrace.plane.normal, planes[0] );
	} else {
		numplanes = 0;
	}

	// never turn against original velocity
	VectorNormalize2( pm->ps->velocity, planes[numplanes] );
	numplanes++;

	for ( bumpcount=0 ; bumpcount < numbumps ; bumpcount++ ) {
		// calculate position we are trying to move to
		VectorMA( pm->ps->origin, time_left, pm->ps->velocity, end );
		// see if we can make it there
		pm->trace ( &trace, pm->ps->origin, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask);

		// entity is completely trapped in another solid
		if (trace.allsolid) {
			pm->ps->velocity[2] = 0;	// don't build up falling damage, but allow sideways acceleration
			return qtrue;
		}

		// actually covered some distance
		if (trace.fraction > 0) { VectorCopy (trace.endpos, pm->ps->origin); }
    // moved the entire distance
		if (trace.fraction == 1) { break; }
		// save entity for contact
		PM_AddTouchEnt( trace.entityNum );

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES) {
			// this shouldn't really happen
			VectorClear( pm->ps->velocity );
			return qtrue;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for ( i = 0 ; i < numplanes ; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[i] ) > 0.99 ) {
				VectorAdd( trace.plane.normal, pm->ps->velocity, pm->ps->velocity );
				break;
			}
		}
		if ( i < numplanes ) { continue; }
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;
		//
		// modify velocity so it parallels all of the clip planes
		//
		// find a plane that it enters
		for ( i = 0 ; i < numplanes ; i++ ) {
			into = DotProduct( pm->ps->velocity, planes[i] );
			if ( into >= 0.1 ) { continue;}  // move doesn't interact with the plane
			// see how hard we are hitting things
			if ( -into > pml.impactSpeed ) {pml.impactSpeed = -into;}
			// slide along the plane
			VectorReflect (pm->ps->velocity, planes[i], clipVelocity, OVERCLIP );
			// slide along the plane
			VectorReflect (endVelocity, planes[i], endClipVelocity, OVERCLIP ); // IoQuake3 Wrapped this behind a gravity check. This version is default q3a

			// see if there is a second plane that the new move enters
			for ( j = 0 ; j < numplanes ; j++ ) {
				if ( j == i ) {continue;}
				if ( DotProduct( clipVelocity, planes[j] ) >= 0.1 ) {continue;}  // move doesn't interact with the plane
				// try clipping the move to the plane
				VectorReflect( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				VectorReflect( endClipVelocity, planes[j], endClipVelocity, OVERCLIP ); //IoQuake3 wrapped this inside a gravity check. This version is default q3a-1.32
				// see if it goes back into the first clip plane
				if ( DotProduct( clipVelocity, planes[i] ) >= 0 ) {continue;}
				// slide the original velocity along the crease
				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, pm->ps->velocity );
				VectorScale( dir, d, clipVelocity );

				if ( gravity ) {
					CrossProduct (planes[i], planes[j], dir);
					VectorNormalize( dir );
					d = DotProduct( dir, endVelocity );
					VectorScale( dir, d, endClipVelocity );
				}

				// see if there is a third plane the the new move enters
				for ( k = 0 ; k < numplanes ; k++ ) {
					if ( k == i || k == j ) {continue;}
					if ( DotProduct( clipVelocity, planes[k] ) >= 0.1 ) {continue;}		// move doesn't interact with the plane
					// stop dead at a tripple plane interaction
					VectorClear( pm->ps->velocity );
					return qtrue;
				}
			}
			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, pm->ps->velocity );  
			VectorCopy( endClipVelocity, endVelocity ); //IoQuake3 wrapped this inside a gravity check. This version is default q3a-1.32
			break;
		}
	}

	if ( gravity ) {VectorCopy( endVelocity, pm->ps->velocity );}
	// don't change velocity if in a timer (FIXME: is this correct?)
	if ( pm->ps->pm_time ) {VectorCopy( primal_velocity, pm->ps->velocity );}
	return ( bumpcount != 0 );
}

//==================
// StepSlideMove
//   Handles stepmove behavior
//==================
// Unmodded Q3A version
void core_StepSlideMove( qboolean gravity ) {
	vec3_t		start_o, start_v;
	//vec3_t		down_o, down_v;
	trace_t		trace;
//	float		down_dist, up_dist;
//	vec3_t		delta, delta2;
	vec3_t		up, down;
	float		stepSize;
	float		delta;
	qboolean	timerActive, cantDoubleJump, isSteepRamp;
	int			max_jumpvel;

	VectorCopy (pm->ps->origin, start_o);
	VectorCopy (pm->ps->velocity, start_v);

	if ( core_SlideMove( gravity ) == 0 ) {return;}		// we got exactly where we wanted to go first try	
	// Step Down
	VectorCopy(start_o, down);
	down[2] -= STEPSIZE;
	pm->trace (&trace, start_o, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	// Step up
	max_jumpvel    = phy_jump_velocity + phy_jump_dj_velocity;
	timerActive    = ( pm->cmd.serverTime - pm->ps->stats[STAT_TIME_LASTJUMP] < phy_jump_dj_time ) ? qtrue : qfalse;
	cantDoubleJump = ( pm->movetype == VQ3 || !timerActive || pm->ps->velocity[2] > max_jumpvel ) ? qtrue : qfalse;
	VectorSet(up, 0, 0, 1);
  isSteepRamp    = DotProduct(trace.plane.normal, up) < 0.7 ? qtrue : qfalse;
	// never step up when:
	//   Step-down trace moved all the way down, (or) we are in a steepramp
  //   (and) still have up velocity
	//   (and) You can't doublejump (vq3 or dj-timer is not active)
	//   (and) Vertical speed is bigger than the maximum possible dj speed (prevent stairs-climb crazyness) (included in cantDoubleJump)
	if (((trace.fraction == 1.0 || isSteepRamp) 
		  && pm->ps->velocity[2] > 0)
		  && cantDoubleJump)
		{ return; }


	//VectorCopy (pm->ps->origin, down_o);
	//VectorCopy (pm->ps->velocity, down_v);

	VectorCopy (start_o, up);
	up[2] += STEPSIZE;

	// test the player position if they were a stepheight higher
	pm->trace (&trace, start_o, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
	if ( trace.allsolid ) {
		if ( pm->debugLevel ) { Com_Printf("%i:bend can't step\n", c_pmove); }
		VectorClear(pm->ps->velocity); // Wallbug fix
		return;		// can't step up
	}

	stepSize = trace.endpos[2] - start_o[2];
	// try slidemove from this position
	VectorCopy (trace.endpos, pm->ps->origin);
	VectorCopy (start_v, pm->ps->velocity);

	core_SlideMove( gravity );

	// push down the final amount
	VectorCopy (pm->ps->origin, down);
	down[2] -= stepSize;
	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	if ( !trace.allsolid )      { VectorCopy (trace.endpos, pm->ps->origin); }
	if ( trace.fraction < 1.0 ) { VectorReflect( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP ); }
	// use the step move
	delta = pm->ps->origin[2] - start_o[2];
	if ( delta > 2 ) {
		if      ( delta < 7 )  { PM_AddEvent( EV_STEP_4 ); }
		else if ( delta < 11 ) { PM_AddEvent( EV_STEP_8 ); }
		else if ( delta < 15 ) { PM_AddEvent( EV_STEP_12 ); }
		else                   { PM_AddEvent( EV_STEP_16 ); }
	}
	if ( pm->debugLevel ) { Com_Printf("%i:stepped\n", c_pmove); }
}