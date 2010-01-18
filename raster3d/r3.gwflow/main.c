
/****************************************************************************
*
* MODULE:       r3.gwflow 
*   	    	
* AUTHOR(S):    Original author 
*               Soeren Gebbert soerengebbert <at> gmx <dot> de
* 		27 11 2006 Berlin
* PURPOSE:      Calculates confined transient three dimensional groundwater flow
*
* COPYRIGHT:    (C) 2006 by the GRASS Development Team
*
*               This program is free software under the GNU General Public
*   	    	License (>=v2). Read the file COPYING that comes with GRASS
*   	    	for details.
*
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/G3d.h>
#include <grass/gmath.h>
#include <grass/glocale.h>
#include <grass/N_pde.h>
#include <grass/N_gwflow.h>


/*- Parameters and global variables -----------------------------------------*/
typedef struct
{
    struct Option *output, *phead, *status, *hc_x, *hc_y, *hc_z, *q, *s, *r,
	*vector_x, *vector_y, *vector_z, *budget, *dt, *maxit, *error, *solver;
    struct Flag *mask;
    struct Flag *full_les;
} paramType;

paramType param;		/*Parameters */

/*- prototypes --------------------------------------------------------------*/
static void set_params(void);	/*Fill the paramType structure */

static void write_result(N_array_3d * status, N_array_3d * phead_start,
			 N_array_3d * phead, double *result,
			 G3D_Region * region, char *name);

/* ************************************************************************* */
/* Set up the arguments we are expecting ********************************** */
/* ************************************************************************* */
void set_params(void)
{
    param.phead = G_define_option();
    param.phead->key = "phead";
    param.phead->type = TYPE_STRING;
    param.phead->required = YES;
    param.phead->gisprompt = "old,grid3,3d-raster";
    param.phead->description = _("The initial piezometric head in [m]");

    param.status = G_define_option();
    param.status->key = "status";
    param.status->type = TYPE_STRING;
    param.status->required = YES;
    param.status->gisprompt = "old,grid3,3d-raster";
    param.status->description =
	_
	("The status for each cell, = 0 - inactive, 1 - active, 2 - dirichlet");

    param.hc_x = G_define_option();
    param.hc_x->key = "hc_x";
    param.hc_x->type = TYPE_STRING;
    param.hc_x->required = YES;
    param.hc_x->gisprompt = "old,grid3,3d-raster";
    param.hc_x->description =
	_("The x-part of the hydraulic conductivity tensor in [m/s]");

    param.hc_y = G_define_option();
    param.hc_y->key = "hc_y";
    param.hc_y->type = TYPE_STRING;
    param.hc_y->required = YES;
    param.hc_y->gisprompt = "old,grid3,3d-raster";
    param.hc_y->description =
	_("The y-part of the hydraulic conductivity tensor in [m/s]");

    param.hc_z = G_define_option();
    param.hc_z->key = "hc_z";
    param.hc_z->type = TYPE_STRING;
    param.hc_z->required = YES;
    param.hc_z->gisprompt = "old,grid3,3d-raster";
    param.hc_z->description =
	_("The z-part of the hydraulic conductivity tensor in [m/s]");

    param.q = G_define_option();
    param.q->key = "q";
    param.q->type = TYPE_STRING;
    param.q->required = NO;
    param.q->gisprompt = "old,grid3,3d-raster";
    param.q->description = _("Sources and sinks in [m^3/s]");

    param.s = G_define_option();
    param.s->key = "s";
    param.s->type = TYPE_STRING;
    param.s->required = YES;
    param.s->gisprompt = "old,grid3,3d-raster";
    param.s->description = _("Specific yield in 1/m");

    param.r = G_define_option();
    param.r->key = "r";
    param.r->type = TYPE_STRING;
    param.r->required = NO;
    param.r->gisprompt = "old,raster,raster";
    param.r->description = _("Recharge raster map in m^3/s");

    param.output = G_define_option();
    param.output->key = "output";
    param.output->type = TYPE_STRING;
    param.output->required = YES;
    param.output->gisprompt = "new,grid3,3d-raster";
    param.output->description = _("The piezometric head result of the numerical calculation will be written to this map");

    param.vector_x = G_define_option();
    param.vector_x->key = "vx";
    param.vector_x->type = TYPE_STRING;
    param.vector_x->required = NO;
    param.vector_x->gisprompt = "new,grid3,3d-raster";
    param.vector_x->description =
	_("Calculate and store the groundwater filter velocity vector part in x direction [m/s]\n");

    param.vector_y = G_define_option();
    param.vector_y->key = "vy";
    param.vector_y->type = TYPE_STRING;
    param.vector_y->required = NO;
    param.vector_y->gisprompt = "new,grid3,3d-raster";
    param.vector_y->description =
	_("Calculate and store the groundwater filter velocity vector part in y direction [m/s]\n");

    param.vector_z = G_define_option();
    param.vector_z->key = "vz";
    param.vector_z->type = TYPE_STRING;
    param.vector_z->required = NO;
    param.vector_z->gisprompt = "new,grid3,3d-raster";
    param.vector_z->description =
	_("Calculate and store the groundwater filter velocity vector part in y direction [m/s]\n");

    param.budget = G_define_option();
    param.budget->key = "budget";
    param.budget->type = TYPE_STRING;
    param.budget->required = NO;
    param.budget->gisprompt = "new,grid3,3d-raster";
    param.budget->description =
	_("Store the groundwater budget for each cell\n");

    param.dt = N_define_standard_option(N_OPT_CALC_TIME);
    param.maxit = N_define_standard_option(N_OPT_MAX_ITERATIONS);
    param.error = N_define_standard_option(N_OPT_ITERATION_ERROR);
    param.solver = N_define_standard_option(N_OPT_SOLVER_SYMM);
    param.solver->options = "cg,pcg,cholesky";

    param.mask = G_define_flag();
    param.mask->key = 'm';
    param.mask->description = _("Use G3D mask (if exists)");

    param.full_les = G_define_flag();
    param.full_les->key = 'f';
    param.full_les->description = _("Use a full filled quadratic linear equation system,"
            " default is a sparse linear equation system.");
}

/* ************************************************************************* */
/* Main function *********************************************************** */
/* ************************************************************************* */
int main(int argc, char *argv[])
{
    struct GModule *module = NULL;
    N_gwflow_data3d *data = NULL;
    N_geom_data *geom = NULL;
    N_les *les = NULL;
    N_les_callback_3d *call = NULL;
    G3D_Region region;
    N_gradient_field_3d *field = NULL;
    N_array_3d *xcomp = NULL;
    N_array_3d *ycomp = NULL;
    N_array_3d *zcomp = NULL;
    double error;
    int maxit;
    const char *solver;
    int x, y, z, stat;

    /* Initialize GRASS */
    G_gisinit(argv[0]);

    module = G_define_module();
    G_add_keyword(_("raster3d"));
    G_add_keyword(_("voxel"));
    G_add_keyword(_("groundwater"));
    G_add_keyword(_("numeric"));
    G_add_keyword(_("simulation"));
    module->description = _("Numerical calculation program for transient, confined groundwater flow in three dimensions");

    /* Get parameters from user */
    set_params();

    /* Have GRASS get pheads */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /*Set the maximum iterations */
    sscanf(param.maxit->answer, "%i", &(maxit));
    /*Set the calculation error break criteria */
    sscanf(param.error->answer, "%lf", &(error));
    /*Set the solver */
    solver = param.solver->answer;

    if (strcmp(solver, G_MATH_SOLVER_DIRECT_CHOLESKY) == 0 && !param.full_les->answer)
	G_fatal_error(_("The cholesky solver dos not work with sparse matrices.\n"
                "You may choose a full filled quadratic matrix, flag -f. "));



    /*Set the defaults */
    G3d_initDefaults();

    /*get the current region */
    G3d_getWindow(&region);

    /*allocate the geometry structure  for geometry and area calculation */
    geom = N_init_geom_data_3d(&region, geom);

    /*Set the function callback to the groundwater flow function */
    call = N_alloc_les_callback_3d();
    N_set_les_callback_3d_func(call, (*N_callback_gwflow_3d));	/*gwflow 3d */

    /*Allocate the groundwater flow data structure */
    data = N_alloc_gwflow_data3d(geom->cols, geom->rows, geom->depths, 0, 0);

    /*Set the calculation time */
    sscanf(param.dt->answer, "%lf", &(data->dt));

    /*read the g3d maps into the memory and convert the null values */
    N_read_rast3d_to_array_3d(param.phead->answer, data->phead,
			      param.mask->answer);
    N_convert_array_3d_null_to_zero(data->phead);
    N_read_rast3d_to_array_3d(param.phead->answer, data->phead_start,
			      param.mask->answer);
    N_convert_array_3d_null_to_zero(data->phead_start);
    N_read_rast3d_to_array_3d(param.status->answer, data->status,
			      param.mask->answer);
    N_convert_array_3d_null_to_zero(data->status);
    N_read_rast3d_to_array_3d(param.hc_x->answer, data->hc_x,
			      param.mask->answer);
    N_convert_array_3d_null_to_zero(data->hc_x);
    N_read_rast3d_to_array_3d(param.hc_y->answer, data->hc_y,
			      param.mask->answer);
    N_convert_array_3d_null_to_zero(data->hc_y);
    N_read_rast3d_to_array_3d(param.hc_z->answer, data->hc_z,
			      param.mask->answer);
    N_convert_array_3d_null_to_zero(data->hc_z);
    N_read_rast3d_to_array_3d(param.q->answer, data->q, param.mask->answer);
    N_convert_array_3d_null_to_zero(data->q);
    N_read_rast3d_to_array_3d(param.s->answer, data->s, param.mask->answer);
    N_convert_array_3d_null_to_zero(data->s);

    /* Set the inactive values to zero, to assure a no flow boundary */
    for (z = 0; z < geom->depths; z++) {
	for (y = 0; y < geom->rows; y++) {
	    for (x = 0; x < geom->cols; x++) {
		stat = (int)N_get_array_3d_d_value(data->status, x, y, z);
		if (stat == N_CELL_INACTIVE) {	/*only inactive cells */
		    N_put_array_3d_d_value(data->hc_x, x, y, z, 0);
		    N_put_array_3d_d_value(data->hc_y, x, y, z, 0);
		    N_put_array_3d_d_value(data->hc_z, x, y, z, 0);
		    N_put_array_3d_d_value(data->s, x, y, z, 0);
		    N_put_array_3d_d_value(data->q, x, y, z, 0);
		}
	    }
	}
    }

    /*assemble the linear equation system */
    if (!param.full_les->answer) {
	les =
	    N_assemble_les_3d(N_SPARSE_LES, geom, data->status, data->phead,
			      (void *)data, call);
    }
    else {
	les =
	    N_assemble_les_3d(N_NORMAL_LES, geom, data->status, data->phead,
			      (void *)data, call);
    }

    if (les && les->type == N_NORMAL_LES) {
	if (strcmp(solver, G_MATH_SOLVER_ITERATIVE_CG) == 0)
	    G_math_solver_cg(les->A, les->x, les->b, les->rows, maxit, error);

	if (strcmp(solver, G_MATH_SOLVER_ITERATIVE_PCG) == 0)
	    G_math_solver_pcg(les->A, les->x, les->b, les->rows, maxit, error,
			      G_MATH_DIAGONAL_PRECONDITION);

	if (strcmp(solver, G_MATH_SOLVER_DIRECT_CHOLESKY) == 0)
	    G_math_solver_cholesky(les->A, les->x, les->b, les->rows,
				   les->rows);
    }
    else if (les && les->type == N_SPARSE_LES) {
	if (strcmp(solver, G_MATH_SOLVER_ITERATIVE_CG) == 0)
	    G_math_solver_sparse_cg(les->Asp, les->x, les->b, les->rows,
				    maxit, error);

	if (strcmp(solver, G_MATH_SOLVER_ITERATIVE_PCG) == 0)
	    G_math_solver_sparse_pcg(les->Asp, les->x, les->b, les->rows,
				     maxit, error, G_MATH_DIAGONAL_PRECONDITION);
    }

    if (les == NULL)
	G_fatal_error(_("Unable to create and solve the linear equation system"));


    /*write the result to the output file and copy the values to the data->phead array */
    write_result(data->status, data->phead_start, data->phead, les->x,
		 &region, param.output->answer);
    N_free_les(les);

    /* Compute the water budget for each cell */
    N_array_3d *budget = N_alloc_array_3d(geom->cols, geom->rows, geom->depths, 1, DCELL_TYPE);
    N_gwflow_3d_calc_water_budget(data, geom, budget);
    
    /*Write the water balance */
    if(param.budget->answer)
    {
	N_write_array_3d_to_rast3d(budget, param.budget->answer, 1);
    }

    /*Compute the the velocity field if required and write the result into three rast3d maps */
    if (param.vector_x->answer || param.vector_y->answer || param.vector_z->answer) {
	field =
	    N_compute_gradient_field_3d(data->phead, data->hc_x, data->hc_y,
					data->hc_z, geom, NULL);

	xcomp =
	    N_alloc_array_3d(geom->cols, geom->rows, geom->depths, 1,
			     DCELL_TYPE);
	ycomp =
	    N_alloc_array_3d(geom->cols, geom->rows, geom->depths, 1,
			     DCELL_TYPE);
	zcomp =
	    N_alloc_array_3d(geom->cols, geom->rows, geom->depths, 1,
			     DCELL_TYPE);

	N_compute_gradient_field_components_3d(field, xcomp, ycomp, zcomp);


        if(param.vector_x->answer)
            N_write_array_3d_to_rast3d(xcomp, param.vector_x->answer, 1);
        if(param.vector_y->answer)
            N_write_array_3d_to_rast3d(ycomp, param.vector_y->answer, 1);
        if(param.vector_z->answer)
            N_write_array_3d_to_rast3d(zcomp, param.vector_z->answer, 1);

	if (xcomp)
	    N_free_array_3d(xcomp);
	if (ycomp)
	    N_free_array_3d(ycomp);
	if (zcomp)
	    N_free_array_3d(zcomp);
	if (field)
	    N_free_gradient_field_3d(field);
    }

    if (data)
	N_free_gwflow_data3d(data);
    if (geom)
	N_free_geom_data(geom);
    if (call)
	G_free(call);

    return (EXIT_SUCCESS);
}


/* ************************************************************************* */
/* this function writes the result from the x vector to a g3d map ********** */
/* ************************************************************************* */
void
write_result(N_array_3d * status, N_array_3d * phead_start,
	     N_array_3d * phead, double *result, G3D_Region * region,
	     char *name)
{
    void *map = NULL;

    int changemask = 0;

    int z, y, x, rows, cols, depths, count, stat;

    double d1 = 0;

    rows = region->rows;
    cols = region->cols;
    depths = region->depths;

    /*Open the new map */
    map = G3d_openCellNew(name, DCELL_TYPE, G3D_USE_CACHE_DEFAULT, region);

    if (map == NULL)
	G3d_fatalError(_("Error opening g3d map <%s>"), name);

    G_message(_("Write the result to g3d map <%s>"), name);

    /*if requested set the Mask on */
    if (param.mask->answer) {
	if (G3d_maskFileExists()) {
	    changemask = 0;
	    if (G3d_maskIsOff(map)) {
		G3d_maskOn(map);
		changemask = 1;
	    }
	}
    }

    count = 0;
    for (z = 0; z < depths; z++) {
	G_percent(z, depths - 1, 10);
	for (y = 0; y < rows; y++) {
	    for (x = 0; x < cols; x++) {
		stat = (int)N_get_array_3d_d_value(status, x, y, z);
		if (stat == N_CELL_ACTIVE) {	/*only active cells */
		    d1 = result[count];
		    /*copy the values */
		    N_put_array_3d_d_value(phead, x, y, z, d1);
		    count++;
		}
		else if (stat == N_CELL_DIRICHLET) {	/*dirichlet cells */
		    d1 = N_get_array_3d_d_value(phead_start, x, y, z);
		}
		else {
		    G3d_setNullValue(&d1, 1, DCELL_TYPE);
		}
		G3d_putDouble(map, x, y, z, d1);
	    }
	}
    }

    /*We set the Mask off, if it was off before */
    if (param.mask->answer) {
	if (G3d_maskFileExists())
	    if (G3d_maskIsOn(map) && changemask)
		G3d_maskOff(map);
    }

    if (!G3d_closeCell(map))
	G3d_fatalError(map, NULL, 0, _("Error closing g3d file"));

    return;
}
