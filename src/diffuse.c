/**************************************************************************\
** File: diffuse.c                                                        **
**                                                                        **
** Purpose: Moves molecules around the world with reactions and collisions**
**                                                                        **
** Testing status: compiles.                                              **
\**************************************************************************/



#include <math.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#include "rng.h"
#include "mem_util.h"
#include "sched_util.h"

#include "mcell_structs.h"
#include "grid_util.h"
#include "vol_util.h"
#include "wall_util.h"
#include "react.h"



#define MULTISTEP_WORTHWHILE 2
#define MULTISTEP_PERCENTILE 0.99
#define MULTISTEP_FRACTION 0.9



extern struct volume *world;


/*************************************************************************
pick_displacement:
  In: vector3 to store the new displacement
      scale factor to apply to the displacement
  Out: No return value.  vector is set to a random orientation and a
         distance chosen from the probability distribution of a diffusing
         molecule, scaled by the scaling factor.
*************************************************************************/

void pick_displacement(struct vector3 *v,double scale)
{
  double r;
  
#if 0
  if (world->fully_random)
  {
    double x,y,z;
    do
    {
      x = 2.0*rng_double(world->seed++)-1.0;
      y = 2.0*rng_double(world->seed++)-1.0;
      z = 2.0*rng_double(world->seed++)-1.0;
    } while (x*x + y*y + z*z >= 1.0 || x*x + y*y + z*z < 0.001);
    r = scale * world->r_step[ rng_uint(world->seed++) & (world->radial_subdivisions-1) ] / sqrt(x*x + y*y + z*z);
    v->x = r*x;
    v->y = r*y;
    v->z = r*z;
    return;
  }
#endif
    
  
  if (world->fully_random)
  {
    double h,r_sin_phi,theta;
    
    r = scale * world->r_step[ rng_uint(world->seed++) & (world->radial_subdivisions-1) ];
    h = 2.0*rng_double(world->seed++) - 1.0;
    theta = 2.0*MY_PI*rng_double(world->seed++);
    
    r_sin_phi = r * sqrt(1.0 - h*h);
    
    v->x = r_sin_phi * cos(theta);
    v->y = r_sin_phi * sin(theta);
    v->z = r*h;
  }
  else
  {
    u_int x_bit,y_bit,z_bit;
    uint r_bit,thetaphi_bit;
    uint bits;
    uint idx;
    
    bits = rng_uint(world->seed++);
    
    x_bit =        (bits & 0x80000000);
    y_bit =        (bits & 0x40000000);
    z_bit =        (bits & 0x20000000);
    thetaphi_bit = (bits & 0x1FFFF000) >> 12;
    r_bit =        (bits & 0x00000FFF);
    
    r = scale * world->r_step[ r_bit & (world->radial_subdivisions - 1) ];
    
    idx = (thetaphi_bit & world->directions_mask);
    while ( idx >= world->num_directions)
    {
      idx = ( rng_uint(world->seed++) & world->directions_mask);
    }
    
    idx *= 3;
    
    if (x_bit) v->x = r * world->d_step[idx];
    else v->x = -r * world->d_step[idx];
    
    if (y_bit) v->y = r * world->d_step[idx+1];
    else v->y = -r * world->d_step[idx+1];

    if (z_bit) v->z = r * world->d_step[idx+2];
    else v->z = -r * world->d_step[idx+2];
  }
}


/*************************************************************************
ray_trace:
  In: molecule that is moving
      linked list of potential collisions with molecules (we could react)
      subvolume that we start in
      displacement vector from current to new location
  Out: collision list of walls and molecules we intersected along our ray
         (current subvolume only)
*************************************************************************/

struct collision* ray_trace(struct molecule *m, struct collision *c,
                            struct subvolume *sv, struct vector3 *v)
{
  struct collision *smash,*shead;
  struct wall_list *wlp;
  struct wall_list fake_wlp;
  double dx,dy,dz,tx,ty,tz,tmin;
  int i,j,k;
  
  shead = NULL;
  smash = (struct collision*) mem_get(sv->mem->coll);
  
  fake_wlp.next = sv->wall_head;
    
  for (wlp = sv->wall_head ; wlp != NULL; wlp = wlp->next)
  {
    i = collide_wall(&(m->pos),v,wlp->this_wall,&(smash->t),&(smash->loc));

    if (i==COLLIDE_REDO)
    {
      if (shead != NULL) mem_put_list(sv->mem->coll,shead);
      shead = NULL;
      wlp = &fake_wlp;
      continue;
    }
    else if (i!=COLLIDE_MISS)
    {
      if (smash->t < tmin) tmin = smash->t;
      smash->what = COLLIDE_WALL + i;
      smash->target = (void*) wlp->this_wall;
      smash->next = shead;
      shead = smash;
      smash = (struct collision*) mem_get(sv->mem->coll);
    }
  }

  if (v->x < 0.0)
  {
    dx = world->x_fineparts[ sv->llf.x ] - m->pos.x;
    i = 0;
  }
  else
  {
    dx = world->x_fineparts[ sv->urb.x ] - m->pos.x;
    i = 1;
  }

  if (v->y < 0.0)
  {
    dy = world->y_fineparts[ sv->llf.y ] - m->pos.y;
    j = 0;
  }
  else
  {
    dy = world->y_fineparts[ sv->urb.y ] - m->pos.y;
    j = 1;
  }

  if (v->z < 0.0)
  {
    dz = world->z_fineparts[ sv->llf.z ] - m->pos.z;
    k = 0;
  }
  else
  {
    dz = world->z_fineparts[ sv->urb.z ] - m->pos.z;
    k = 1;
  }
  
  tx = dx * v->y * v->z;
  ty = v->x * dy * v->z;
  tz = v->x * v->y * dz;
  if (tx<0.0) tx = -tx;
  if (ty<0.0) ty = -ty;
  if (tz<0.0) tz = -tz;
  
  if (tx < ty)
  {
    if (tx < tz)
    {
      smash->t = dx / v->x;
      smash->what = COLLIDE_SUBVOL + COLLIDE_SV_NX + i;
    }
    else
    {
      smash->t = dz / v->z;
      smash->what = COLLIDE_SUBVOL + COLLIDE_SV_NZ + k;
    }
  }
  else
  {
    if (ty < tz)
    {
      smash->t = dy / v->y;
      smash->what = COLLIDE_SUBVOL + COLLIDE_SV_NY + j;
    }
    else
    {
      smash->t = dz / v->z;
      smash->what = COLLIDE_SUBVOL + COLLIDE_SV_NZ + k;
    }
  }
  
  smash->loc.x = m->pos.x + smash->t * v->x;
  smash->loc.y = m->pos.y + smash->t * v->y;
  smash->loc.z = m->pos.z + smash->t * v->z;
  
  smash->target = sv;
  smash->next = shead;
  shead = smash;

  for ( ; c!=NULL ; c = c->next)
  {
    i = collide_mol(&(m->pos),v,(struct abstract_molecule*)c->target,
                    &(c->t),&(c->loc));
    if (i != COLLIDE_MISS)
    {
      smash = (struct collision*) mem_get(sv->mem->coll);
      memcpy(smash,c,sizeof(struct collision));
      
      smash->what = i + COLLIDE_MOL;

      smash->next = shead;
      shead = smash;
    }
  }
  
  return shead;
}


void tell_loc(struct molecule *m,char *s)
{
  if (0 || s[0] == '\0')
  printf("%sMy name is %x and I live at %.3f,%.3f,%.3f\n",
         s,(int)m,m->pos.x*world->length_unit,m->pos.y*world->length_unit,m->pos.z*world->length_unit);
}


double estimate_disk(struct vector3 *loc,struct vector3 *mv,double R,struct subvolume *sv)
{
  struct vector3 u,v;
  double d2_mv_i,a,b,t;
  double upperU = 1.0;
  double upperV = 1.0;
  double lowerU = 1.0;
  double lowerV = 1.0;
  struct wall_list *wl;
  
  pick_displacement(&u,1.0);
  d2_mv_i = 1.0/(mv->x*mv->x + mv->y*mv->y + mv->z*mv->z);
  a = d2_mv_i*(u.x*mv->x + u.y*mv->y + u.z*mv->z);
  u.x = u.x - a*mv->x;
  u.y = u.y - a*mv->y;
  u.z = u.z - a*mv->z;
  b = R/sqrt(u.x*u.x + u.y*u.y + u.z*u.z);
  u.x *= b;
  u.y *= b;
  u.z *= b;
  a = sqrt(d2_mv_i);
  v.x = a*(mv->y*u.z - mv->z*u.y);
  v.y = a*(-mv->x*u.z + mv->z*u.x);
  v.z = a*(mv->x*u.y - mv->y*u.x);
  
/* FIXME--this is horribly inefficient; should create modified collide_wall */

  for (wl = sv->wall_head ; wl!=NULL ; wl = wl->next)
  {
    t = touch_wall(loc,&u,wl->this_wall);
    if (t>0.0 && t<upperU) upperU = t;
    if (t<0.0 && -t<lowerU) lowerU = -t;
    
    t = touch_wall(loc,&v,wl->this_wall);
    if (t>0.0 && t<upperV) upperV = t;
    if (t<0.0 && -t<lowerV) lowerV = -t;
  }


  if (u.x > EPS_C)
  {
    u.x = 1/u.x;
    t = (world->x_fineparts[sv->urb.x] - loc->x)*u.x;
    if (t < upperU) upperU = t;
    t = (loc->x - world->x_fineparts[sv->llf.x])*u.x;
    if (t < lowerU) lowerU = t;
  }
  else if (u.x < EPS_C)
  {
    u.x = 1/u.x;
    t = (world->x_fineparts[sv->llf.x] - loc->x)*u.x;
    if (t < upperU) upperU = t;
    t = (loc->x - world->x_fineparts[sv->urb.x])*u.x;
    if (t < lowerU) lowerU = t;
  }
  if (u.y > EPS_C)
  {
    u.y = 1/u.y;
    t = (world->y_fineparts[sv->urb.y] - loc->y)*u.y;
    if (t < upperU) upperU = t;
    t = (loc->y - world->y_fineparts[sv->llf.y])*u.y;
    if (t < lowerU) lowerU = t;
  }
  else if (u.y < EPS_C)
  {
    u.y = 1/u.y;
    t = (world->y_fineparts[sv->llf.y] - loc->y)*u.y;
    if (t < upperU) upperU = t;
    t = (loc->y - world->y_fineparts[sv->urb.y])*u.y;
    if (t < lowerU) lowerU = t;
  }
  if (u.z > EPS_C)
  {
    u.z = 1/u.z;
    t = (world->z_fineparts[sv->urb.z] - loc->z)*u.z;
    if (t < upperU) upperU = t;
    t = (loc->z - world->z_fineparts[sv->llf.z])*u.z;
    if (t < lowerU) lowerU = t;
  }
  else if (u.z < EPS_C)
  {
    u.z = 1/u.z;
    t = (world->z_fineparts[sv->llf.z] - loc->z)*u.z;
    if (t < upperU) upperU = t;
    t = (loc->z - world->z_fineparts[sv->urb.z])*u.z;
    if (t < lowerU) lowerU = t;
  }

  if (v.x > EPS_C)
  {
    v.x = 1/v.x;
    t = (world->x_fineparts[sv->urb.x] - loc->x)*v.x;
    if (t < upperV) upperV = t;
    t = (loc->x - world->x_fineparts[sv->llf.x])*v.x;
    if (t < lowerV) lowerV = t;
  }
  else if (v.x < EPS_C)
  {
    v.x = 1/v.x;
    t = (world->x_fineparts[sv->llf.x] - loc->x)*v.x;
    if (t < upperV) upperV = t;
    t = (loc->x - world->x_fineparts[sv->urb.x])*v.x;
    if (t < lowerV) lowerV = t;
  }
  if (v.y > EPS_C)
  {
    v.y = 1/v.y;
    t = (world->y_fineparts[sv->urb.y] - loc->y)*v.y;
    if (t < upperV) upperV = t;
    t = (loc->y - world->y_fineparts[sv->llf.y])*v.y;
    if (t < lowerV) lowerV = t;
  }
  else if (v.y < EPS_C)
  {
    v.y = 1/v.y;
    t = (world->y_fineparts[sv->llf.y] - loc->y)*v.y;
    if (t < upperV) upperV = t;
    t = (loc->y - world->y_fineparts[sv->urb.y])*v.y;
    if (t < lowerV) lowerV = t;
  }
  if (v.z > EPS_C)
  {
    v.z = 1/v.z;
    t = (world->z_fineparts[sv->urb.z] - loc->z)*v.z;
    if (t < upperV) upperV = t;
    t = (loc->z - world->z_fineparts[sv->llf.z])*v.z;
    if (t < lowerV) lowerV = t;
  }
  else if (v.z < EPS_C)
  {
    v.z = 1/v.z;
    t = (world->z_fineparts[sv->llf.z] - loc->z)*v.z;
    if (t < upperV) upperV = t;
    t = (loc->z - world->z_fineparts[sv->urb.z])*v.z;
    if (t < lowerV) lowerV = t;
  }

  a = 4.0/(upperU*upperU + lowerU*lowerU + upperV*upperV + lowerV*lowerV);
/*
  if (a > 1.1) printf("Correction factor %.2f\n",a);
  if (a < 1.0-EPS_C) printf("MUDDY BLURDER! a=%.2f R=%.2f u=[%.2f %.2f %.2f] %.2f %.2f %.2f %.2f\n",
                            a,R,u.x,u.y,u.z,upperU,lowerU,upperV,lowerV);
*/
  return a;
}


int search_schedule_for_me(struct schedule_helper *sch,struct abstract_element *ae)
{
  struct abstract_element *aep;
  int i;
  
  for ( aep = sch->current ; aep != NULL ; aep = aep->next )
  {
    if (aep == ae) return 1;
  }
  for (i=0;i<sch->buf_len;i++)
  {
    for ( aep = sch->circ_buf_head[i] ; aep!=NULL ; aep = aep->next )
    {
      if (aep == ae) return 1;
    }
  }
  
  if (sch->next_scale == NULL) return 0;
  else return search_schedule_for_me(sch->next_scale , ae);
}


int search_memory_for_me(struct mem_helper *mh,struct abstract_list *al)
{
  int i;
  
  for (i=0;i<mh->buf_len;i++)
  {
    if (al == (struct abstract_list*)(mh->storage + mh->record_size*i)) return 1;
  }
  
  if (mh->next_helper == NULL) return 0;
  else return search_memory_for_me(mh->next_helper,al);
}

int test_subvol_for_circular(struct subvolume *sv)
{
  struct molecule *mp,*smp,*psmp;
  int warned = 0;
  
  psmp = NULL;
  mp = smp = sv->mol_head;
  do
  {
    if (!warned && smp->subvol != sv)
    {
      printf("Occupancy leak from %x to %x through %x to %x\n",(int)sv,(int)smp->subvol,(int)psmp,(int)smp);
      warned = 1;
    }
    psmp = smp;
    smp = smp->next_v;
    mp = mp->next_v;
    if (mp!=NULL) mp = mp->next_v;
  } while (mp != NULL && smp != NULL && mp != smp);
  
  if (mp != NULL) return 1;
  return 0;
}
  
  

/*************************************************************************
diffuse_3D:
  In: molecule that is moving
      maximum time we can spend diffusing
      are we inert (nonzero) or can we react (zero)?
  Out: Pointer to the molecule if it still exists (may have been
       reallocated), NULL otherwise.
       Position and time are updated, but molecule is not rescheduled.
*************************************************************************/

struct molecule* diffuse_3D(struct molecule *m,double max_time,int inert)
{
  struct vector3 displacement;
  struct collision *smash,*shead,*shead2;
  struct subvolume *sv;
  struct wall_list *wl;
  struct wall *w;
  struct rxn *r;
  struct molecule *mp,*old_mp;
  struct grid_molecule *g;
  struct abstract_molecule *am;
  struct species *sm;
  double d2;
  double d2_nearmax;
  double d2min = GIGANTIC;
  double steps=1.0;
  double factor;
  double rate_factor=1.0;
  
  int i,j,k,l;
  
  int calculate_displacement = 1;

#if 0  
  double lo = 1.0/world->length_unit;
  double hi = 2.0/world->length_unit;
#endif
  
  sm = m->properties;
  if (sm==NULL) printf("BROKEN!!!!!\n");
  if (sm->space_step <= 0.0)
  {
    m->t += max_time;
    return m;
  }
  
#if 0
  if (m->subvol != find_subvolume(&(m->pos),m->subvol))
  {
    printf("U POSITION: LOST (%x at t=%.3e)\n",(int)m,m->subvol->mem->current_time);
  }
  if (test_subvol_for_circular(m->subvol))
  {
    printf("U MOLECULES: RINGED [%x by %x]\n",(int)m->subvol,(int)m);
    printf("  [%d]",m->subvol->mol_count); mp = m->subvol->mol_head;
    for (i=0;i<100;i++)
    {
      printf(" %x",(int)mp);
      mp = mp->next_v;
      if (mp==NULL) break;
    }
    printf("\n");
  }
#endif

#if 0
  if (m->pos.x < lo || m->pos.x > hi || m->pos.y < lo || m->pos.y > hi ||
      m->pos.z < lo || m->pos.z > hi)
  {
/*
    printf("U BOXIN': LEAKY (%.3e %.3e %.3e not in [%.3e %.3e]\n",
           m->pos.x,m->pos.y,m->pos.z,lo,hi);
*/
  }
#endif


#if 1
  while (m->next_v != NULL && m->next_v->properties == NULL)
  {
    mp = m->next_v;
    if ((mp->flags & TYPE_GRID)!=0) printf("Huh?!\n");
    m->next_v = mp->next_v;
    
    if ((mp->flags & IN_MASK)==IN_VOLUME)
    {
#if 0
      if (search_schedule_for_me(mp->subvol->mem->timer,(struct abstract_element*)mp))
      {
        printf("ERRORROROROROR!!\n");
        printf("Flags are %x\n",mp->flags);
      }
      if (!search_memory_for_me(mp->subvol->mem->mol,(struct abstract_list*)mp))
      {
        printf("Idiotocracy!\n");
      }
#endif
      mem_put(mp->birthplace,mp);
    }
    else if ((mp->flags & IN_VOLUME) != 0)
    {
      mp->flags -=IN_VOLUME;
      mp->next_v = NULL;
    }
  }
#endif
  
pretend_to_call_diffuse_3D:   /* Label to allow fake recursion */

  sv = m->subvol;
  
  
/* Even if we can't react, let's do a little bit of clean up. */
/* We'll just clear out anyone after us who is defunct and */
/* not worry about the whole list.  (Tom's idea, impl. by Rex) */


/* Done housekeeping, now let's do something fun! */    
  
  shead = NULL;
  old_mp = NULL;
  if ( (m->properties->flags & CAN_MOLMOL) != 0 )
  {
    for (mp = sv->mol_head ; mp != NULL ; old_mp = mp , mp = mp->next_v)
    {
continue_special_diffuse_3D:   /* Jump here instead of looping if old_mp,mp already set */

      if (mp==m) continue;
      if (m->properties!=sm)
      {
        if (calculate_displacement) printf("WHAAA????\n");
        else printf("HWAAAAA????\n");
      }
      
      if (mp->properties == NULL)  /* Reclaim storage */
      {
        if (old_mp != NULL) old_mp->next_v = mp->next_v;
        else sv->mol_head = mp->next_v;
        
        if ((mp->flags & IN_MASK)==IN_VOLUME) mem_put(mp->birthplace,mp);
        else if ((mp->flags & IN_VOLUME) != 0) mp->flags -= IN_VOLUME;
        
        mp = mp->next_v;

        if (mp==NULL) break;
        else goto continue_special_diffuse_3D;  /*continue without incrementing pointer*/
      }
      
      r = trigger_bimolecular(
              m->properties->hashval,mp->properties->hashval,
              (struct abstract_molecule*)m,(struct abstract_molecule*)mp,0,0
            );
      
      if (r != NULL)
      {
        smash = mem_get(sv->mem->coll);
        smash->target = (void*) mp;
        smash->intermediate = r;
        smash->next = shead;
        smash->what = COLLIDE_MOL;
        shead = smash;
      }
    }
  }
  
  if (calculate_displacement)
  {
    if (max_time > MULTISTEP_WORTHWHILE)
    {
      d2_nearmax = sm->space_step * world->r_step[ (int)(world->radial_subdivisions * MULTISTEP_PERCENTILE) ];
      d2_nearmax *= d2_nearmax;
    
      if ( (m->properties->flags & CAN_MOLMOL) != 0 )
      {
        for (smash = shead ; smash != NULL ; smash = smash->next)
        {
          mp = (struct molecule*)smash->target;
          d2 = (m->pos.x - mp->pos.x)*(m->pos.x - mp->pos.x) +
               (m->pos.y - mp->pos.y)*(m->pos.x - mp->pos.y) +
               (m->pos.z - mp->pos.z)*(m->pos.x - mp->pos.z);
          if (d2 < d2min) d2min = d2;
        }
      }
      for (wl = sv->wall_head ; wl!=NULL ; wl = wl->next)
      {
        w = wl->this_wall;
        d2 = w->normal.x*m->pos.x + w->normal.y*m->pos.y + w->normal.z*m->pos.z;
        d2 *= d2;
        if (d2 < d2min) d2min = d2;
      }
    
      d2 = (m->pos.x - world->x_fineparts[ sv->llf.x ]);
      d2 *= d2;
      if (d2 < d2min) d2min = d2;
      
      d2 = (m->pos.x - world->x_fineparts[ sv->urb.x ]);
      d2 *= d2;
      if (d2 < d2min) d2min = d2;

      d2 = (m->pos.y - world->y_fineparts[ sv->llf.y ]);
      d2 *= d2;
      if (d2 < d2min) d2min = d2;
      
      d2 = (m->pos.y - world->y_fineparts[ sv->urb.y ]);
      d2 *= d2;
      if (d2 < d2min) d2min = d2;

      d2 = (m->pos.z - world->z_fineparts[ sv->llf.z ]);
      d2 *= d2;
      if (d2 < d2min) d2min = d2;
      
      d2 = (m->pos.z - world->z_fineparts[ sv->urb.z ]);
      d2 *= d2;
      if (d2 < d2min) d2min = d2;
      
      if (d2min < d2_nearmax) steps = 1.0;
      else if ( d2_nearmax*max_time < d2min ) steps = (1.0+EPS_C)*max_time;
      else steps = d2min / d2_nearmax;
      
      if (steps < MULTISTEP_WORTHWHILE) steps = 1.0;
    }
    else if (max_time < MULTISTEP_FRACTION) steps = max_time;
    else steps = 1.0;
    
    if (steps == 1.0)
    {
      pick_displacement(&displacement,sm->space_step);
      rate_factor = 1.0;
    }
    else
    {
      rate_factor = sqrt(steps);
      pick_displacement(&displacement,rate_factor*sm->space_step);
    }

    world->diffusion_number += 1.0;
    world->diffusion_cumsteps += steps;
  }
  
  d2 = displacement.x*displacement.x + displacement.y*displacement.y +
       displacement.z*displacement.z;
       
#if 0
  if (sqrt(d2)*world->length_unit > 0.5)
  {
    if (calculate_displacement)
      printf("Yikes, molecule %x wants to travel %.3f on seed %d!\n",
             (int)m,sqrt(d2),world->seed-1);
    else
      printf("Yowzers, molecule %x wants to travel %.3f more!\n",
             (int)m,sqrt(d2));
  }
#endif
  
  do
  {
    shead2 = ray_trace(m,shead,sv,&displacement);
    
    shead2 = (struct collision*)ae_list_sort((struct abstract_element*)shead2);
    
    for (smash = shead2; smash != NULL; smash = smash->next)
    {
      
      if (smash->t >= 1.0 || smash->t < 0.0)
      {
        smash = NULL;
        break;
      }
      
      if (smash->next != NULL && smash->next->t - smash->t < 10*EPS_C &&
          (smash->what & COLLIDE_WALL)==0 && (smash->next->what & COLLIDE_WALL)!=0)
      {
        struct collision *temp;
        temp = smash->next;
        smash->next = temp->next;
        temp->next = smash;
        smash = temp;
      }

      if ( (smash->what & COLLIDE_MOL) != 0 && !inert )
      {
        m->collisions++;

        am = (struct abstract_molecule*)smash->target;
        if ((am->flags & ACT_INERT) != 0)
        {
          if (smash->t < am->t + am->t2) continue;
        }

        factor = rate_factor / estimate_disk(
          &(smash->loc),&displacement,world->rx_radius_3d,m->subvol
        );
        
        i = test_bimolecular(smash->intermediate,factor);
        if (i<0) continue;
        
        j = outcome_bimolecular(
                smash->intermediate,i,(struct abstract_molecule*)m,
                am,0,0,m->t+steps*smash->t,&(smash->loc)
              );
        
        if (j) continue;
        else
        {
          if (shead2 != NULL) mem_put_list(sv->mem->coll,shead2);
          if (shead != NULL) mem_put_list(sv->mem->coll,shead);
          return NULL;
        }
      }
      else if ( (smash->what & COLLIDE_WALL) != 0 )
      {
        w = (struct wall*) smash->target;
        
        if ( (smash->what & COLLIDE_MASK) == COLLIDE_FRONT ) k = 1;
        else k = -1;
        
        if ( w->effectors != NULL && (m->properties->flags&CAN_MOLGRID) != 0 )
        {
          j = xyz2grid( &(smash->loc) , w->effectors );
          if (w->effectors->mol[j] != NULL)
          {
            if (m->index != j || m->releaser != w->effectors)
            {
            g = w->effectors->mol[j];
            r = trigger_bimolecular(
              m->properties->hashval,g->properties->hashval,
              (struct abstract_molecule*)m,(struct abstract_molecule*)g,
              k,g->orient
            );
            if (r!=NULL)
            {
              i = test_bimolecular(r,rate_factor * w->effectors->binding_factor);
              if (i >= 0)
              {
                l = outcome_bimolecular(
                  r,i,(struct abstract_molecule*)m,
                  (struct abstract_molecule*)g,
                  k,g->orient,m->t+steps*smash->t,&(smash->loc)
                );
        
                if (l==0)
                {
                  if (shead2 != NULL) mem_put_list(sv->mem->coll,shead2);
                  if (shead != NULL) mem_put_list(sv->mem->coll,shead);
                  return NULL;
                }
              }
            }
            }
            else
            {
/*              printf("Nope!  I'm not rebinding.\n");  */
              m->index = -1;
            }
          }
        }
        
        if ( (m->properties->flags&CAN_MOLWALL) != 0 )
        {
          m->index = -1;
          r = trigger_intersect(
                  m->properties->hashval,(struct abstract_molecule*)m,k,w
                );
          
          if (r != NULL)
          {
            i = test_intersect(r,rate_factor);
            if (i>=0)
            {
              j = outcome_intersect(
                      r,i,w,(struct abstract_molecule*)m,
                      k,m->t + steps*smash->t,&(smash->loc)
                    );
              if (j==1) continue; /* pass through */
              else if (j==0)
              {
                if (shead2 != NULL) mem_put_list(sv->mem->coll,shead2);
                if (shead != NULL) mem_put_list(sv->mem->coll,shead);
                return NULL;
              }
            }
          }
        }
        /* default is to reflect */

        m->pos.x += displacement.x * smash->t;
        m->pos.y += displacement.y * smash->t;
        m->pos.z += displacement.z * smash->t;
        tell_loc(m,"Boing!!  ");
        m->t += steps*smash->t;
        m->path_length += sqrt( smash->t*smash->t*
                                ( displacement.x * displacement.x +
                                  displacement.y * displacement.y +
                                  displacement.z * displacement.z ) );
        steps *= (1.0-smash->t);
        
        factor = -2.0 * (displacement.x*w->normal.x + displacement.y*w->normal.y + displacement.z*w->normal.z);
        displacement.x = (displacement.x + factor*w->normal.x) * (1.0-smash->t);
        displacement.y = (displacement.y + factor*w->normal.y) * (1.0-smash->t);
        displacement.z = (displacement.z + factor*w->normal.z) * (1.0-smash->t);
        
        break;
      }
      else if ((smash->what & COLLIDE_SUBVOL) != 0)
      {
        struct subvolume *nsv;
        
        m->path_length += sqrt( (m->pos.x - smash->loc.x)*(m->pos.x - smash->loc.x)
                               +(m->pos.y - smash->loc.y)*(m->pos.y - smash->loc.y)
                               +(m->pos.z - smash->loc.z)*(m->pos.z - smash->loc.z));
        tell_loc(m,"Whoosh!  ");

#if 0
        if ((fabs(m->pos.x)-10.0)>EPS_C ||
            (fabs(m->pos.y)-10.0)>EPS_C ||
            (fabs(m->pos.z)-10.0)>EPS_C)
        {
          struct wall_list *twlp;
          printf("  We shouldn't be whooshing at %.3f!\n",smash->t);
          printf("  We are: %.3f %.3f %.3f -> %.3f %.3f %.3f\n",
                 m->pos.x,m->pos.y,m->pos.z,
                 smash->loc.x,smash->loc.y,smash->loc.z);
          printf("  LLF corner: %.3f %.3f %.3f\n",
                 world->x_fineparts[sv->llf.x],
                 world->y_fineparts[sv->llf.y],
                 world->z_fineparts[sv->llf.z]);
          printf("  URB corner: %.3f %.3f %.3f\n",
                 world->x_fineparts[sv->urb.x],
                 world->y_fineparts[sv->urb.y],
                 world->z_fineparts[sv->urb.z]);
          for (twlp = sv->wall_head; twlp != NULL; twlp = twlp->next)
          {
            printf("    Wall %x: [%.2f %.2f %.2f] [%.2f %.2f %.2f] [%.2f %.2f %.2f]\n",
                   (int)twlp->this_wall,
                   twlp->this_wall->vert[0]->x,
                   twlp->this_wall->vert[0]->y,
                   twlp->this_wall->vert[0]->z,
                   twlp->this_wall->vert[1]->x,
                   twlp->this_wall->vert[1]->y,
                   twlp->this_wall->vert[1]->z,
                   twlp->this_wall->vert[2]->x,
                   twlp->this_wall->vert[2]->y,
                   twlp->this_wall->vert[2]->z);
          }
        }
#endif

        m->pos.x = smash->loc.x;
        m->pos.y = smash->loc.y;
        m->pos.z = smash->loc.z;
        displacement.x *= (1.0-smash->t);
        displacement.y *= (1.0-smash->t);
        displacement.z *= (1.0-smash->t);
        
        m->t += steps*smash->t;
        steps *= (1.0-smash->t);
        if (steps < EPS_C) steps = EPS_C;

        nsv = traverse_subvol(sv,&(m->pos),smash->what - COLLIDE_SV_NX - COLLIDE_SUBVOL);
        if (nsv==NULL)
        {
          m->properties->population--;
          m->properties = NULL;
          if (shead2 != NULL) mem_put_list(sv->mem->coll,shead2);
          if (shead != NULL) mem_put_list(sv->mem->coll,shead);
          
          return NULL;
        }
        else
        {
          struct molecule *mm;
          struct species *mms;
/*          printf("Moving molecule %x from subvolume %x to %x ",
                 (int)m,(int)sv,(int)nsv);*/
          mms = m->properties;
          mm = m;
          if (m->subvol == NULL) printf("WE DO NOT EXIST!!\n");
          if (m->subvol == nsv) printf("CRAZY!  Moving from a volume into itself!\n");
          m = migrate_molecule(m,nsv);
          if (m->properties==NULL)
          {
            printf("We nulled, used to be %x with %x, now %x with NULL!\n",(int)mm,(int)mms,(int)m);
            if (calculate_displacement) printf("(First pass.)\n");
          }
          
/*          printf("and identity is now %x.\n",(int)m);*/
        }

        if (shead2 != NULL) mem_put_list(sv->mem->coll,shead2);
        if (shead != NULL) mem_put_list(sv->mem->coll,shead);
        
        calculate_displacement = 0;
        if (m->properties==NULL) printf("This molecule shouldn't be jumping.\n");
        goto pretend_to_call_diffuse_3D;  /* Jump to beginning of function */        
      }
    }
    
    if (shead2 != NULL) mem_put_list(sv->mem->coll,shead2);
  }
  while (smash != NULL);
  
  if (shead != NULL) mem_put_list(sv->mem->coll,shead);
  m->pos.x += displacement.x;
  m->pos.y += displacement.y;
  m->pos.z += displacement.z;
  tell_loc(m,"Whew...  ");
  m->t += steps;
  m->path_length += sqrt( displacement.x*displacement.x +
                          displacement.y*displacement.y +
                          displacement.z*displacement.z );
  m->index = -1;
  return m;
}
    


/*************************************************************************
run_timestep:
  In: local storage area to use
      time of the next release event
      time of the next checkpoint
  Out: No return value.  Every molecule in the subvolume is updated in
       position and rescheduled at least one timestep ahead.  The
       current_time of the subvolume is also incremented.
*************************************************************************/

void run_timestep(struct storage *local,double release_time,double checkpt_time)
{
  struct abstract_molecule *a;
  struct rxn *r;
  double t;
  double stop_time,max_time;
  int i,j;
  
  while ( (a = (struct abstract_molecule*)schedule_next(local->timer)) != NULL )
  {

    if (a->properties == NULL)  /* Defunct!  Remove molecule. */
    {
      if ((a->flags & IN_MASK) == IN_SCHEDULE)
      {
        a->next = NULL; 
        mem_put(a->birthplace,a);
      }
      else a->flags -= IN_SCHEDULE;
      
      continue;
    }

    a->flags -= IN_SCHEDULE;
    
    if (local->current_time + local->max_timestep < checkpt_time) stop_time = local->max_timestep;
    else stop_time = checkpt_time - local->current_time;
    
    if (a->t2 < EPS_C || a->t2 < EPS_C*a->t)
    {
      if ((a->flags & (ACT_INERT+ACT_NEWBIE)) != 0)
      {
        a->flags -= (a->flags & (ACT_INERT + ACT_NEWBIE));
        if ((a->flags & ACT_REACT) != 0)
        {
          r = trigger_unimolecular(a->properties->hashval,a);
          a->t2 = timeof_unimolecular(r);
        }
      }
      else if ((a->flags & ACT_REACT) != 0)
      {
        r = trigger_unimolecular(a->properties->hashval,a);
        i = which_unimolecular(r);
        j = outcome_unimolecular(r,i,a,a->t);
        if (j) /* We still exist */
        {
          a->t2 = timeof_unimolecular(r);
        }
        else continue;
      }
    }

    if ((a->flags & (ACT_INERT+ACT_REACT)) == 0) max_time = stop_time;
    else if (a->t2 < stop_time) max_time = a->t2;
    else max_time = stop_time;
    
    if ((a->flags & ACT_DIFFUSE) != 0)
    {
      if ((a->flags & TYPE_3D) != 0)
      {
        if (max_time > release_time - local->current_time) max_time = release_time - local->current_time;
        t = a->t;
        a = (struct abstract_molecule*)diffuse_3D((struct molecule*)a , max_time , a->flags & ACT_INERT);
        if (a!=NULL) /* We still exist */
        {
          a->t2 -= a->t - t;
        }
        else continue;
      }
      else  /* Surface diffusion not yet implemented */
      {
        a->t += max_time;
        a->t2 -= max_time;
      }
    }
    else
    {
      if (a->t2==0) a->t = checkpt_time;
      else if (a->t2 + a->t + EPS_C < checkpt_time)
      {
        a->t += a->t2;
        a->t2 = 0;
      }
      else
      {
        a->t2 -= checkpt_time - a->t;
        a->t = checkpt_time;
      }
    }

    a->flags += IN_SCHEDULE;
    if (a->flags & TYPE_GRID) schedule_add(local->timer,a);
    else schedule_add(((struct molecule*)a)->subvol->mem->timer,a);
  }
  
  local->current_time += 1.0;
}
