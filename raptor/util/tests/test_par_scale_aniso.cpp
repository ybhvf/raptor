// Copyright (c) 2015-2017, RAPtor Developer Team
// License: Simplified BSD, http://opensource.org/licenses/BSD-2-Clause
#include "gtest/gtest.h"
#include "raptor.hpp"

using namespace raptor;

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);
    ::testing::InitGoogleTest(&argc, argv);
    int temp = RUN_ALL_TESTS();
    MPI_Finalize();
    return temp;
} // end of main() //

TEST(ParAnisoSpMVTest, TestsInUtil)
{
    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    FILE* f;
    double b_val;
    int grid[2] = {25, 25};
    double eps = 0.001;
    double theta = M_PI/8.0;
    double* stencil = diffusion_stencil_2d(eps, theta);


    ParCSRMatrix* A = par_stencil_grid(stencil, grid, 2);
    ParCSRMatrix* A_scale = A->copy();

    aligned_vector<double> scales;

    ParVector x(A->global_num_cols, A->on_proc_num_cols, A->partition->first_local_col);
    ParVector x_scale(A->global_num_cols, A->on_proc_num_cols, A->partition->first_local_col);
    ParVector b(A->global_num_rows, A->local_num_rows, A->partition->first_local_row);
    ParVector b_scale(A->global_num_rows, A->local_num_rows, A->partition->first_local_row);
    ParVector r(A->global_num_rows, A->local_num_rows, A->partition->first_local_row);

    ParMultilevel* ml;

    double r_norm;
    double r_norm_scaled;
    int iter;

    // Form right-hand-side
    x.set_const_value(1.0);
    A->mult(x, b);
    for (int i = 0; i < A->local_num_rows; i++)
    {
        b_scale[i] = b[i];
    }

    // Solve Standard AMG
    x.set_const_value(0.0);
    ml = new ParRugeStubenSolver();
    ml->setup(A);
    iter = ml->solve(x, b);
    delete ml;
    A->residual(x, b, r);
    r_norm = r.norm(2);
    if (rank == 0) printf("Original Rnorm: %e (%d iterations)\n", r_norm, iter);

    // Diagonally Scale System
    diagonally_scale(A_scale, b_scale, scales);

    // Solve Diagonally Scaled System
    x_scale.set_const_value(0.0);
    ml = new ParRugeStubenSolver();
    ml->setup(A_scale);
    iter = ml->solve(x_scale, b_scale);
    delete ml;
    A_scale->residual(x_scale, b_scale, r);
    r_norm_scaled = r.norm(2);
    if (rank == 0) printf("Scaled Rnorm: %e (%d iterations)\n", r_norm_scaled, iter);

    // Unscale solution
    diagonally_unscale(x_scale, scales);
    A->residual(x_scale, b, r);
    r_norm_scaled = r.norm(2);
    if (rank == 0) printf("Unscaled Rnorm: %e (%d iterations)\n", r_norm_scaled, iter);

    delete A;
    delete[] stencil;

} // end of TEST(ParAnisoSpMVTest, TestsInUtil) //

