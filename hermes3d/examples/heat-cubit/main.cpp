#define H3D_REPORT_WARN
#define H3D_REPORT_INFO
#define H3D_REPORT_VERBOSE
#include "config.h"
//#include <getopt.h>
#include <hermes3d.h>

// Solving a simple heat equation to demonstrate how to use CUBIT with Hermes3D.
//
// Use mesh file 'cylinder2.e. Material IDs corresponds to elements markers, 
// sideset IDs correspond to face (BC) markers.
//
//  The following parameters can be changed:
const int INIT_REF_NUM = 0;                       // Number of initial uniform mesh refinements.
const int P_INIT_X = 1,
          P_INIT_Y = 1,
          P_INIT_Z = 1;                           // Initial polynomial degree of all mesh elements.
bool solution_output = true;                      // Generate output files (if true).
MatrixSolverType matrix_solver = SOLVER_UMFPACK;  // Possibilities: SOLVER_AMESOS, SOLVER_MUMPS, SOLVER_NOX, 
                                                  // SOLVER_PARDISO, SOLVER_PETSC, SOLVER_UMFPACK.
const char* iterative_method = "bicgstab";        // Name of the iterative method employed by AztecOO (ignored
                                                  // by the other solvers). 
                                                  // Possibilities: gmres, cg, cgs, tfqmr, bicgstab.
const char* preconditioner = "jacobi";            // Name of the preconditioner employed by AztecOO (ignored by
                                                  // the other solvers). 
                                                  // Possibilities: none, jacobi, neumann, least-squares, or a
                                                  //  preconditioner from IFPACK (see solver/aztecoo.h)                                                  


// Boundary condition types. 
BCType bc_types(int marker)
{
  if (marker == 1) return BC_ESSENTIAL;
  else return BC_NATURAL;
}

// Essential (Dirichlet) boundary condition values. 
scalar essential_bc_values(int ess_bdy_marker, double x, double y, double z)
{
  return 10;
}

// Weak forms.
#include "forms.cpp"

// Solution output.
void out_fn(MeshFunction *fn, const char *name)
{
  char fname[1024];
  sprintf(fname, "%s.vtk", name);
  FILE *f = fopen(fname, "w");
  if (f != NULL) {
    VtkOutputEngine vtk(f);
    vtk.out(fn, name);
    fclose(f);
  }
  else warning("Could not open file '%s' for writing.", fname);
}

// Boundary conditions output.
void out_bc(Mesh *mesh, const char *name)
{
  char of_name[1024];
  FILE *ofile;
  sprintf(of_name, "%s.vtk", name);
  ofile = fopen(of_name, "w");
  if (ofile != NULL) {
    VtkOutputEngine output(ofile);
    output.out_bc(mesh, name);
    fclose(ofile);
  }
  else warning("Can not open '%s' for writing.", of_name);
}

int main(int argc, char **args)
{
  // Time measurement.
  TimePeriod cpu_time;
  cpu_time.tick();

  // Load the mesh. 
  Mesh mesh;
  ExodusIIReader mesh_loader;
  if (!mesh_loader.load("cylinder2.e", &mesh))
    error("Loading mesh file '%s' failed.\n", "cylinder2.e");

  // Perform initial mesh refinements.
  for (int i=0; i < INIT_REF_NUM; i++) mesh.refine_all_elements(H3D_H3D_H3D_REFT_HEX_XYZ);
  
  // Create H1 space with default shapeset.
  H1Space space(&mesh, bc_types, essential_bc_values, Ord3(P_INIT_X, P_INIT_Y, P_INIT_Z));
  printf("Number of DOF: %d.\n", space.get_num_dofs());

  // Initialize the weak formulation.
  WeakForm wf;
  wf.add_matrix_form(callback(bilinear_form1), HERMES_SYM, 1);
  wf.add_matrix_form(callback(bilinear_form2), HERMES_SYM, 2);
  wf.add_vector_form(callback(linear_form), HERMES_ANY);

  initialize_solution_environment(matrix_solver, argc, args);

  // Assemble the linear problem.
  info("Assembling the linear problem (ndof: %d).", Space::get_num_dofs(&space));
  bool is_linear = true;
  DiscreteProblem dp(&wf, &space, is_linear);
  SparseMatrix* matrix = create_matrix(matrix_solver);
  Vector* rhs = create_vector(matrix_solver);
  Solver* solver = create_linear_solver(matrix_solver, matrix, rhs);

  if (matrix_solver == SOLVER_AZTECOO) 
  {
    ((AztecOOSolver*) solver)->set_solver(iterative_method);
    ((AztecOOSolver*) solver)->set_precond(preconditioner);
    // Using default iteration parameters (see solver/aztecoo.h).
  }

  dp.assemble(matrix, rhs);
	
  // Solve the linear system of the reference problem. If successful, obtain the solution.
  info("Solving the linear problem.");
  Solution sln(space.get_mesh());
  if(solver->solve()) Solution::vector_to_solution(solver->get_solution(), &space, &sln);
  else error ("Matrix solver failed.\n");

  // Output solution and mesh.
  if (solution_output) 
  {
    out_fn(&sln, "sln");
    out_bc(&mesh, "bc");
  }

  // Time measurement.
  cpu_time.tick();

  // Print timing information.
  info("Solution saved. Total running time: %g s", cpu_time.accumulated());

  // Clean up.
  delete matrix;
  delete rhs;
  delete solver;
  finalize_solution_environment(matrix_solver);

  return 0;
}