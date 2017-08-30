#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

enum returnID {
	success_help = -1,
	success = 0,

	overd_err = 1,
	underd_err = 2,
	zero_err = 3,
	gen_err = 4,
	conflict_err = 5,
	vect_nan = 6,
	known_err = 7,
	other_err = 8
};

struct program_args {
	// general
	int precision;
	bool quiet, list, numbers_force, flags;
	std::string delim = "\n";
	// intern
	long long number;
	long double lbound, ubound;
	std::string generator;
	// rounding
	bool ceil, floor, round, trunc;
	// matcher
	std::vector<long double> excluded, included;
	bool norepeat;
	std::vector<std::string> prefix, suffix, contains;
	// stats
	bool stat_all, stat_min, stat_max, stat_median,
		stat_avg, stat_var, stat_std, stat_coef;
};

returnID parse_args(program_args & args, int argc, char const * const * argv) {
	static auto const ld_prec = std::numeric_limits<long double>::max_digits10;

	namespace po = boost::program_options;
	po::options_description general("General options");
	general.add_options()
		("help,h", "produce this help message")
		("precision,p", po::value<int>(&args.precision)->default_value(ld_prec), 
			"output precision (not internal precision)")
		("quiet,q", po::bool_switch(&args.quiet)->default_value(false),
			"disable number output, useful with stats")
		("list", po::bool_switch(&args.list)->default_value(false),
			"print numbers in a list")
		("delim", po::value<std::string>(&args.delim),
			"change the delimiter")
		("numbers-force", po::bool_switch(&args.numbers_force)->default_value(false),
			"force the count of numbers printed to be equal to --number")
		("flags", po::bool_switch(&args.flags)->default_value(false),
			"print the flags");

	po::options_description intern("Internal RNG options");
	intern.add_options()
		("number,n", po::value<long long>(&args.number)->default_value(1),
			"count of numbers to be generated")
		("lbound,l", po::value<long double>(&args.lbound)->default_value(0.0),
			"minimum number (ldouble)")
		("ubound,u", po::value<long double>(&args.ubound)->default_value(1.0),
			"maximum number (ldouble)")
		("generator,g", po::value<std::string>(&args.generator)->default_value("mt19937"),
			"change the RNG algorithm:\nminstd_rand0, minstd_rand"
			"\nmt19937, mt19937_64\nranlux24_base, ranlux48_base"
			"\nranlux24, ranlux48\nknuth_b, default_random_engine"
			"\nbadrandom (std::rand)");

	po::options_description rounding("Rounding options");
	rounding.add_options()
		("ceil,c", po::bool_switch(&args.ceil)->default_value(false),
			"apply ceiling function")
		("floor,f", po::bool_switch(&args.floor)->default_value(false),
			"apply floor function")
		("round,r", po::bool_switch(&args.round)->default_value(false),
			"apply round function")
		("trunc,t", po::bool_switch(&args.trunc)->default_value(false),
			"apply truncation");

	po::options_description matcher("Matcher options");
	matcher.add_options()
		("exclude", po::value<std::vector<long double> >(&args.excluded)->multitoken(), 
			"print only the numbers not exactly specified, best with rounding")
		("include", po::value<std::vector<long double> >(&args.included)->multitoken(),
			"print only the numbers exactly specified, best with rounding ")
		("norepeat", po::bool_switch(&args.norepeat)->default_value(false),
			"exclude repeated numbers from being printed, best with rounding")
		("prefix", po::value<std::vector<std::string> >(&args.prefix)->multitoken(),
			"only print if the number begins with string(s)")
		("suffix", po::value<std::vector<std::string> >(&args.suffix)->multitoken(),
			"only print if the number ends with string(s)")
		("contains", po::value<std::vector<std::string> >(&args.contains)->multitoken(),
			"only print if the number contains string(s)");

	po::options_description stats("Statistics options");
	stats.add_options()
		("stat-all", po::bool_switch(&args.stat_all)->default_value(false),
			"print all statistics")
		("stat-min", po::bool_switch(&args.stat_min)->default_value(false), 
			"print the minimum")
		("stat-max", po::bool_switch(&args.stat_max)->default_value(false),
			"print the maximum")
		("stat-median", po::bool_switch(&args.stat_median)->default_value(false),
			"print the median")
		("stat-avg", po::bool_switch(&args.stat_avg)->default_value(false),
			"print the average")
		("stat-var", po::bool_switch(&args.stat_var)->default_value(false),
			"print the variance")
		("stat-std", po::bool_switch(&args.stat_std)->default_value(false),
			"print the standard deviation")
		("stat-coef", po::bool_switch(&args.stat_coef)->default_value(false),
			"print the coefficient of variation");

	po::options_description all("Allowed options");
	all.add(general).add(intern).add(rounding).add(matcher).add(stats);

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, all), vm);
	po::notify(vm);

	if(vm.count("help")) {
		std::cout << all << '\n';
		return returnID::success_help;
	}

	if(args.precision > ld_prec) {
		std::cerr << "error: --precision cannot be greater than the precision"
			" for <long double> (" << ld_prec << ")\n";
		return returnID::overd_err;
	}

	if(args.precision <= -1) {
		std::cerr << "error: --precision cannot be < 0\n";
		return returnID::underd_err;
	}

	if(args.number <= 0) {
		std::cerr << "error: the argument for option '--number' is invalid"
			" (must be >= 1)\n";
		return returnID::zero_err;
	}

	const std::vector<std::string> gen_opts {{"minstd_rand0", "minstd_rand",
		"mt19937", "mt19937_64", "ranlux24_base", "ranlux48_base", "ranlux24",
		"ranlux48", "knuth_b", "default_random_engine", "badrandom"}};
	if(std::find(gen_opts.begin(), gen_opts.end(), args.generator) == gen_opts.end()) {
		std::cerr << "error: --generator must be: minstd_rand0, minstd_rand, "
			"mt19937, mt19937_64, ranlux24_base, ranlux48_base, "
			"ranlux24, ranlux48, knuth_b, default_random_engine, badrandom\n";
		return returnID::gen_err;
	}

	if(args.ceil + args.floor + args.round + args.trunc > 1) {
		std::cerr << "error: --ceil, --floor, --round, and --trunc"
			" are mutually exclusive\n";
		return returnID::conflict_err;
	}

	if(args.ceil || args.floor || args.round || args.trunc) {
		args.precision = 0;
	}
	
	std::vector<std::vector<std::string> > filters = {{args.prefix, args.suffix, args.contains}};
	for(auto i : filters) {
		for(auto j : i) {
			if(j.find_first_not_of("0123456789.") != std::string::npos || std::count(std::begin(j), std::end(j), '.') > 1) {
				std::cerr << "error: --prefix, --suffix, and --contains can only be numbers\n";
				return returnID::vect_nan;
			}
		}
	}

	return returnID::success;
}

template<typename GEN = std::mt19937, typename DIST = std::uniform_real_distribution<long double> >
std::function<long double()> r_gen(const program_args & args) {
	GEN generator{std::random_device{}()};
	DIST dis{args.lbound, args.ubound};
	return [dis, generator]() mutable -> auto { return dis(generator); };
}

long double random(const program_args & args) {
	if(args.generator == "minstd_rand0") return r_gen<std::minstd_rand0>(args)();
	else if(args.generator == "minstd_rand") return r_gen<std::minstd_rand>(args)();
	else if(args.generator == "mt19937_64") return r_gen<std::mt19937_64>(args)();
	else if(args.generator == "ranlux24_base") return r_gen<std::ranlux24_base>(args)();
	else if(args.generator == "ranlux48_base") return r_gen<std::ranlux48_base>(args)();
	else if(args.generator == "ranlux24") return r_gen<std::ranlux24>(args)();
	else if(args.generator == "ranlux48") return r_gen<std::ranlux48>(args)();
	else if(args.generator == "knuth_b") return r_gen<std::knuth_b>(args)();
	else if(args.generator == "default_random_engine") return r_gen<std::default_random_engine>(args)();
	else if(args.generator == "badrandom")
		return args.lbound + (std::rand() / (RAND_MAX / (args.ubound - args.lbound)));
	else return r_gen<std::mt19937>(args)();
}

bool filter(const long double rand, const int precision,
		const std::vector<std::string> & fx,
		bool(*predicate)(const std::string&, const std::string&)) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(precision) << rand;
	const auto str_rand = oss.str();
	return std::none_of(fx.begin(), fx.end(), [&](auto const & s) { return predicate(str_rand, s); });
}

int main(int argc, char* argv[]) {
	try {
		program_args args;

		switch(auto result = parse_args(args, argc, argv)) {
			case returnID::success: break;
			case returnID::success_help: return 0;
			default: return result;
		}

		std::vector<long double> generated;

		std::cout.precision(args.precision);

		if(args.generator == "badrandom") std::srand(std::time(nullptr));

		long long list_cnt = 0;

		for(long long i = 1; i <= args.number;) {
			if(!args.numbers_force) ++i;
			if(args.list) ++list_cnt;

			long double rand = random(args);

			if(args.ceil) rand = std::ceil(rand);
			else if(args.floor) rand = std::floor(rand);
			else if(args.round) rand = std::round(rand);
			else if(args.trunc) rand = std::trunc(rand);

			if(!args.excluded.empty() && std::find(args.excluded.begin(), args.excluded.end(), rand) != args.excluded.end())
				continue;
			else if(!args.included.empty() && std::find(args.included.begin(), args.included.end(), rand) == args.included.end())
				continue;
			else if(args.norepeat && std::find(generated.begin(), generated.end(), rand) != generated.end())
				continue;
			else if(!args.prefix.empty() && filter(rand, args.precision, args.prefix, boost::starts_with))
				continue;
			else if(!args.suffix.empty() && filter(rand, args.precision, args.suffix, boost::ends_with))
				continue;
			else if(!args.contains.empty() && filter(rand, args.precision, args.contains, boost::contains))
				continue;

			generated.push_back(rand);

			if(!args.quiet) {
				if(args.list && args.numbers_force) std::cout << i << ". ";
				if(args.list) std::cout << list_cnt << ". ";
				std::cout << std::fixed << rand << args.delim;
				if(args.numbers_force) ++i;
			}
		}

		if(args.delim != "\n" && !args.quiet) std::cout << '\n';

		if((args.stat_all || args.stat_min || args.stat_max || args.stat_median || args.stat_avg
			|| args.stat_var || args.stat_std || args.stat_coef) && !args.quiet)
			std::cout << '\n';

		if(args.stat_all || args.stat_min || args.stat_max) {
			auto minmax = std::minmax_element(generated.begin(), generated.end());
			if(args.stat_all || args.stat_min)
				std::cout << std::fixed << "min: " << *minmax.first << '\n';
			if(args.stat_all || args.stat_max)
				std::cout << std::fixed << "max: " << *minmax.second << '\n';
		}

		if(args.stat_all || args.stat_median) {
			auto midpoint = generated.begin() + generated.size() / 2;
			std::nth_element(generated.begin(), midpoint, generated.end());
			auto median = *midpoint;
			if(generated.size() % 2 == 0)
				median = (median + *std::max_element(generated.begin(), midpoint)) / 2;
			std::cout << std::fixed << "median: " << median << '\n';
		}

		if(args.stat_all || args.stat_avg) {
			long double avg = std::accumulate(generated.begin(), generated.end(), 0.0) / generated.size();
			std::cout << std::fixed << "avg: " << avg << '\n';
		}

		if(args.stat_all || args.stat_var) {
			long double avg = std::accumulate(generated.begin(), generated.end(), 0.0) / generated.size();
			long double var = 0.0;
			for(const auto & i : generated) var += std::pow(i - avg, 2);
			std::cout << std::fixed << "variance: " << var / generated.size() << '\n';
		}

		if(args.stat_all || args.stat_std) {
			long double avg = std::accumulate(generated.begin(), generated.end(), 0.0) / generated.size();
			long double std = 0.0;
			for(const auto & i : generated) std += std::pow(i - avg, 2);
			std::cout << std::fixed << "standard deviation: " << std::sqrt(std / generated.size()) << '\n';
		}

		if(args.stat_all || args.stat_coef) {
			long double avg = std::accumulate(generated.begin(), generated.end(), 0.0) / generated.size();
			long double std = 0.0;
			for(const auto & i : generated) std += std::pow(i - avg, 2);
			std::cout << std::fixed << "coefficient of variation: " << std::sqrt(std / generated.size()) / avg << '\n';
		}

		if(args.flags) {
			std::cout << "\nFlags:\n - General options:\n\thelp: 0"
				<< "\n\tprecision: " << args.precision
				<< "\n\tquiet: " << args.quiet
				<< "\n\tlist: " << args.list
				<< "\n\tnumbers-force: " << args.numbers_force
				<< "\n\tflags: 1"
				<< "\n\tdelim: " << args.delim
				<< "\n - Internal RNG options:"
				<< "\n\tnumber: " << args.number
				<< "\n\tlbound: " << args.lbound
				<< "\n\tubound: " << args.ubound
				<< "\n\tgenerator: " << args.generator
				<< "\n - Rounding options:"
				<< "\n\tceil: " << args.ceil
				<< "\n\tfloor: " << args.floor
				<< "\n\tround: " << args.round
				<< "\n\ttrunc: " << args.trunc
				<< "\n - Matcher options:"
				<< "\n\texclude: ";
			for(const auto & i : args.excluded) std::cout << i << ' ';
			std::cout << "\n\tinclude: ";
			for(const auto & i : args.included) std::cout << i << ' ';
			std::cout << "\n\tnorepeat: " << args.norepeat
				<< "\n\tprefix: ";
			for(const auto & i : args.prefix) std::cout << i << ' ';
			std::cout << "\n\tsuffix: ";
			for(const auto & i : args.suffix) std::cout << i << ' ';
			std::cout << "\n\tcontains: ";
			for(const auto & i : args.contains) std::cout << i << ' ';
			std::cout << "\n - Statistics options:"
				<< "\n\tstat-all: " << args.stat_all
				<< "\n\tstat-min: " << args.stat_min
				<< "\n\tstat-max: " << args.stat_max
				<< "\n\tstat-median: " << args.stat_median
				<< "\n\tstat-avg: " << args.stat_avg
				<< "\n\tstat-var: " << args.stat_var
				<< "\n\tstat-std: " << args.stat_std
				<< "\n\tstat-coef: " << args.stat_coef << '\n';
		}

		return returnID::success;

	} catch(std::exception & e) {
		std::cerr << "error: " << e.what() << '\n';
		return returnID::known_err;

	} catch(...) {
		std::cerr << "error: exception of unknown type!\n";
		return returnID::other_err;
	}
}
