#ifndef MCELL_REACT
#define MCELL_REACT

#include "mcell_structs.h"


/* In react_trig.c */
struct rxn* trigger_unimolecular(u_int hash,struct abstract_molecule *reac);
struct rxn* trigger_surface_unimol(struct abstract_molecule *reac,struct wall *w);
int trigger_bimolecular_preliminary(u_int hashA,u_int hashB,
  struct species *reacA,struct species *reacB);
int trigger_trimolecular_preliminary(u_int hashA, u_int hashB, u_int hashC,
  struct species *reacA, struct species *reacB, struct species *reacC);
int trigger_bimolecular(u_int hashA,u_int hashB,
  struct abstract_molecule *reacA,struct abstract_molecule *reacB,
  short orientA,short orientB, struct rxn **matching_rxns);
int trigger_trimolecular(u_int hashA,u_int hashB, u_int hashC,
  struct species *reacA,struct species *reacB,
  struct species *reacC, int orientA, int orientB, int orientC, 
  struct rxn **matching_rxns);
struct rxn* trigger_intersect(u_int hashA,struct abstract_molecule *reacA,
  short orientA,struct wall *w);


/* In react_cond.c */
double timeof_unimolecular(struct storage *local, struct rxn *rx, struct abstract_molecule *a);
double timeof_special_unimol(struct storage *local, struct rxn *rxuni,struct rxn *rxsurf, struct abstract_molecule *a);
int which_unimolecular(struct storage *local, struct rxn *rx, struct abstract_molecule *a);
int is_surface_unimol(struct storage *local, struct rxn *rxuni, struct rxn *rxsurf,  struct abstract_molecule *a);
int test_bimolecular(struct storage *local, struct rxn *rx, double scaling, double local_prob_factor, struct abstract_molecule *a1, struct abstract_molecule *a2);
int test_many_bimolecular(struct storage *local, struct rxn **rx, double *scaling, int n, int *chosen_pathway, struct abstract_molecule **complexes, int *complex_limits);
int test_many_bimolecular_all_neighbors(struct storage *local, struct rxn **rx, double *scaling, double local_prob_factor, int n, int *chosen_pathway, struct abstract_molecule **complexes, int *complex_limits);
int test_intersect(struct storage *local, struct rxn *rx, double scaling);
void check_probs(struct storage *local, struct rxn *rx, double t);


/* In react_outc.c */
int outcome_unimolecular(struct storage *local,
                         struct rxn *rx,
                         int path,
                         struct abstract_molecule *reac,
                         double t);
int outcome_bimolecular(struct storage *local,
                        struct rxn *rx,
                        int path,
                        struct abstract_molecule *reacA,
                        struct abstract_molecule *reacB,
                        short orientA,
                        short orientB,
                        double t,
                        struct vector3 *hitpt,
                        struct vector3 *loc_okay);
int outcome_trimolecular(struct rng_state *rng,
                         struct rxn *rx,
                         int path,
                         struct abstract_molecule *reacA,
                         struct abstract_molecule *reacB,
                         struct abstract_molecule *reacC,
                         short orientA,
                         short orientB, 
                         short orientC,
                         double t,
                         struct vector3 *hitpt,
                         struct vector3 *loc_okay);
int outcome_intersect(struct storage *local,
                      struct rxn *rx,
                      int path,
                      struct wall *surface,
                      struct abstract_molecule *reac,
                      short orient,
                      double t,
                      struct vector3 *hitpt,
                      struct vector3 *loc_okay);
int is_compatible_surface(void *req_species, struct wall *w);

#endif
