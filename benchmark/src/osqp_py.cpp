#include <string>
#include <cnpy.hpp>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <chrono>
#include <fmt/ranges.h>

#include <fstream>

#include <qp/eq_solver.hpp>
#include <qp/precond/ruiz.hpp>
#include <chrono>

#include <osqp.hpp>

using Scalar = c_float;
using namespace ldlt;

auto main() -> int {
	Scalar eps_abs = Scalar(1e-9);
	Scalar eps_rel = 0;
	i32 max_iter = 1000;

	std::ifstream file(INRIA_LDLT_QP_PYTHON_PATH "osqp_source_files.txt");

	std::string path_len;
	std::string path;
	path_len.resize(32);

	file.read(&path_len[0], 32);
	file.get(); // '\n'
	std::fflush(stdout);
	i32 n_files = i32(std::stol(path_len));

	for (i32 i = 0; i < n_files; ++i) {
		file.read(&path_len[0], 32);
		file.get(); // ':'
		i32 ipath_len = i32(std::stol(path_len));
		path.resize(usize(ipath_len));
		file.read(&path[0], ipath_len);
		file.get(); // '\n'

		Qp<Scalar> qp{
				from_data,
				cnpy::npy_load_mat<double>(path + "/H.npy").cast<Scalar>(),
				cnpy::npy_load_vec<double>(path + "/g.npy").cast<Scalar>(),
				cnpy::npy_load_mat<double>(path + "/A.npy").cast<Scalar>(),
				cnpy::npy_load_vec<double>(path + "/b.npy").cast<Scalar>(),
		};
		i32 n = i32(qp.H.rows());
		i32 n_eq = i32(qp.A.rows());

		Vec<Scalar> primal_init = Vec<Scalar>::Zero(n);
		Vec<Scalar> dual_init = Vec<Scalar>::Zero(n_eq);

		auto ruiz =
				qp::preconditioner::RuizEquilibration<Scalar, colmajor, colmajor>{
						n,
						n_eq,
				};

		auto duration = std::chrono::seconds{1};
		auto ours = ldlt_test::bench_for(duration, [&] {
			primal_init.setZero();
			dual_init.setZero();
			return qp::detail::solve_qp( //
					detail::from_eigen_vector_mut(primal_init),
					detail::from_eigen_vector_mut(dual_init),
					qp.as_view(),
					max_iter,
					eps_abs,
					eps_rel,
					ruiz);
		});

		auto osqp = ldlt_test::bench_for(duration, [&] {
			return ldlt_test::osqp::solve_eq_osqp_sparse( //
					detail::from_eigen_vector_mut(primal_init),
					detail::from_eigen_vector_mut(dual_init),
					ldlt_test::osqp::to_sparse_sym(qp.H),
					ldlt_test::osqp::to_sparse(qp.A),
					qp.as_view().g,
					qp.as_view().b,
					max_iter,
					eps_abs,
					eps_rel);
		});

		fmt::print(
				"n: {}, n_eq: {}\n"
				"ours : {:>4} iters, {:>15}\n"
				"osqp : {:>4} iters, {:>15}\n\n",
				n,
				n_eq,
				ours.result.n_iters,
				ours.duration,
				osqp.result,
				osqp.duration);
	}
}
