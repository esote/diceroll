#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

enum returnID {success = 0, known_err = 1, other_err = 2, zero_err = 3, 
	conflict_err = 4, overd_err = 5, underd_err = 6, exclude_err = 7};

const int ld_prec = std::numeric_limits<long double>::max_digits10;

enum fx_enum {prefix = 0, suffix = 1, contains = 2};
bool fx_vect(const long double & rand, const std::vector<std::string> & fx, const int & state);

int main(int ac, char* av[]) {
	try {
		long long number;
		long double lbound, ubound;
		bool ceil, floor, round, trunc; // mutually exclusive
		int precision;
		std::vector<long double> excluded;
		bool norepeat, stat_min, stat_max, stat_median, stat_avg, bad_random, list, quiet;
		std::vector<std::string> prefix, suffix, contains;
		std::string delim = "\n";
		std::string dist;

		namespace po = boost::program_options;
		po::options_description desc("Options");
		desc.add_options()
			("help,h", "produce this help message")
			("number,n", po::value<long long>(&number)->default_value(1),
				"count of numbers to be generated")
			("lbound,l", po::value<long double>(&lbound)->default_value(0.0),
				"minimum number (ldouble) to be generated")
			("ubound,u", po::value<long double>(&ubound)->default_value(1.0),
				"maximum number (ldouble) to be generated")
			("ceil,c", po::bool_switch(&ceil)->default_value(false),
				"apply ceiling function to numbers")
			("floor,f", po::bool_switch(&floor)->default_value(false),
				"apply floor function to numbers")
			("round,r", po::bool_switch(&round)->default_value(false),
				"apply round function to numbers")
			("trunc,t", po::bool_switch(&trunc)->default_value(false),
				"apply truncation to numbers")
			("precision,p", po::value<int>(&precision)->default_value(ld_prec), 
				"output precision (not internal precision, cannot be > ldouble precision)")
			("exclude,e", po::value<std::vector<long double> >(&excluded)->multitoken(), 
				"exclude numbers from being printed, best with --ceil, --floor, --round, or --trunc")
			("norepeat,x", po::bool_switch(&norepeat)->default_value(false), 
				"exclude repeated numbers from being printed, best with --ceil, --floor, --round, or --trunc")
			("stat-min", po::bool_switch(&stat_min)->default_value(false), 
				"print the lowest value generated")
			("stat-max", po::bool_switch(&stat_max)->default_value(false),
				"print the highest value generated")
			("stat-median", po::bool_switch(&stat_median)->default_value(false),
				"print the median of the values generated")
			("stat-avg", po::bool_switch(&stat_avg)->default_value(false),
				"print the average of the values generated")
			("bad-random", po::bool_switch(&bad_random)->default_value(false),
				"use srand(time(NULL)) and rand() for generating random numbers (limited by RAND_MAX)")
			("prefix", po::value<std::vector<std::string> >(&prefix)->multitoken(),
				"only print when the number begins with string(s)")
			("suffix", po::value<std::vector<std::string> >(&suffix)->multitoken(),
				"only print when the number ends with string(s)")
			("contains", po::value<std::vector<std::string> >(&contains)->multitoken(),
				"only print when the number contains string(s)")
			("list", po::bool_switch(&list)->default_value(false),
				"print numbers in a list with positional numbers prefixed")
			("delim", po::value<std::string>(&delim),
				"change the delimiter")
			("quiet", po::bool_switch(&quiet)->default_value(false),
				"disable number output, useful when paired with stats")
			("dist", po::value<std::string>(&dist)->default_value("uniform_real_distribution"),
				"choose a different distribution");

		po::variables_map vm;
		po::store(po::parse_command_line(ac, av, desc), vm);
		po::notify(vm);

		if(vm.count("help")) {
			std::cout << desc << '\n';
			return returnID::success;

		} else if(number <= 0) {
			std::cerr << "error: the argument for option '--number' is invalid (n must be >= 1)\n";
			return returnID::zero_err;

		} else if(ceil + floor + round + trunc > 1) {
			std::cerr << "error: --ceil, --floor, --round, and --trunc are mutually exclusive and may only be called once\n";
			return returnID::conflict_err;

		} else if(precision > ld_prec) {
			std::cerr << "error: --precision cannot be greater than the precision for <long double> (" 
				<< ld_prec << ")\n";
			return returnID::overd_err;

		} else if(precision <= -1) {
			std::cerr << "error: --precision cannot be less than zero\n";
			return returnID::underd_err;

		} else if(vm.count("exclude") && vm["exclude"].empty()) {
			std::cerr << "error: --exclude was specified without arguments (arguments are separated by spaces)\n";
			return returnID::exclude_err;

		} else {
			std::vector<long double> repeated, generated;

			std::random_device rd;
			std::mt19937 generator(rd());
			if(dist == "uniform_int_distribution")
				std::uniform_int_distribution<long double> dis(lbound, ubound);
			else if(dist == "uniform_real_distribution")
				std::uniform_real_distribution<long double> dis(lbound, ubound);
			else if(dist == "bernoulli_distribution")
				std::bernoulli_distribution<long double> dis(lbound, ubound);
			else if(dist == "binomial_distribution")
				std::binomial_distribution<long double> dis(lbound, ubound);
			else if(dist == "negative_binomial_distribution")
				std::negative_binomial_distribution<long double> dis(lbound, ubound);
			else if(dist == "geometric_distribution")
				std::geometric_distribution<long double> dis(lbound, ubound);
			else if(dist == "poisson_distribution")
				std::poisson_distribution<long double> dis(lbound, ubound);
			else if(dist == "exponential_distribution")
				std::exponential_distribution<long double> dis(lbound, ubound);
			else if(dist == "gamma_distribution")
				std::gamma_distribution<long double> dis(lbound, ubound);
			else if(dist == "weibull_distribution")
				std::weibull_distribution<long double> dis(lbound, ubound);
			else if(dist == "extreme_value_distribution")
				std::extreme_value_distribution<long double> dis(lbound, ubound);
			else if(dist == "normal_distribution")
				std::normal_distribution<long double> dis(lbound, ubound);

			if(bad_random) std::srand(std::time(NULL));

			std::cout.precision(precision);

			long long list_cnt = 1;

			for(long long i = 1; i <= number; i++) {
				if(list) ++list_cnt;
				long double rand = (bad_random) ? lbound + (std::rand() / (RAND_MAX / (ubound - lbound))) : dis(generator);

				if(ceil) rand = std::ceil(rand);
				else if(floor) rand = std::floor(rand);
				else if(round) rand = std::round(rand);
				else if(trunc) rand = std::trunc(rand);

				if(vm.count("exclude") && std::find(excluded.begin(), excluded.end(), rand) != excluded.end())
					continue;
				else if(norepeat && std::find(repeated.begin(), repeated.end(), rand) != repeated.end())
					continue;
				else if(vm.count("prefix") && fx_vect(rand, prefix, fx_enum::prefix))
					continue;
				else if(vm.count("suffix") && fx_vect(rand, suffix, fx_enum::suffix))
					continue;
				else if(vm.count("contains") && fx_vect(rand, contains, fx_enum::contains))
					continue;

				if(list && !quiet) std::cout << list_cnt << ".\t";

				if(!quiet) std::cout << std::fixed << rand << delim;

				if(norepeat) repeated.push_back(rand);

				generated.push_back(rand);
			}

			if(delim != "\n" && !quiet) std::cout << '\n';

			if((stat_min || stat_max || stat_median || stat_avg) && !quiet)
				std::cout << '\n';

			if(stat_min)
				std::cout << "min: " << *std::min_element(generated.begin(), generated.end()) << '\n';
			if(stat_max)
				std::cout << "max: " << *std::max_element(generated.begin(), generated.end()) << '\n';
			if(stat_median) {
				std::sort(generated.begin(), generated.end());
				auto median = (generated.size() % 2) ? generated[generated.size() / 2]
					: (generated[generated.size() / 2 - 1] + generated[generated.size() / 2]) / 2;
				std::cout << "median: " << median << '\n';
			}
			if(stat_avg) {
				long double sum = 0.0;
				for(auto i : generated)
					sum += i;
				std::cout << "avg: " << sum / generated.size() << '\n';
			}

			return returnID::success;
		}

	} catch(std::exception & e) {
		std::cerr << "error: " << e.what() << '\n';
		return returnID::known_err;

	} catch(...) {
		std::cerr << "error: exception of unknown type!\n";
		return returnID::other_err;
	}
}

bool fx_vect(const long double & rand, const std::vector<std::string> & fx, const int & state) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(ld_prec) << rand;
	std::string str_rand = oss.str();
	for(auto i : fx) {
		if((state == fx_enum::prefix && boost::starts_with(str_rand, i))
			|| (state == fx_enum::suffix && boost::ends_with(str_rand, i))
			|| (state == fx_enum::contains && boost::contains(str_rand, i))) return false;
	}
	return true;
}
