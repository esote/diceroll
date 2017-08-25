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
	success = 0,
	known_err = 1,
	other_err = 2,
	zero_err = 3, 
	conflict_err = 4,
	overd_err = 5,
	underd_err = 6,
	exclude_err = 7,
	round_prec = 8,
	vect_nan = 9,
	gen_err = 10,
	dist_err = 11,
	success_help = -1
};

bool filter(const long double rand, const int precision, const std::vector<std::string> & fx, bool(*predicate)(const std::string&, const std::string&)) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(precision) << rand;
	const auto str_rand = oss.str();
	return std::none_of(fx.begin(), fx.end(), [&](auto const & s) { return predicate(str_rand, s); });
}


struct program_args {
	long long number;
	long double lbound, ubound;
	bool ceil, floor, round, trunc; // mutually exclusive
	int precision;
	std::vector<long double> excluded;
	bool norepeat, stat_min, stat_max, stat_median,
		stat_avg, bad_random, list, quiet, numbers_force,
		flags;
	std::vector<std::string> prefix, suffix, contains;
	std::string delim = "\n", generator = "mt19937", dist = "uniform_real_distribution";
};

returnID parse_args(program_args & args, int argc, char const * const * argv) {
	static auto const ld_prec = std::numeric_limits<long double>::max_digits10;

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
		("help,h", "produce this help message")
		("number,n", po::value<long long>(&args.number)->default_value(1),
			"count of numbers to be generated")
		("lbound,l", po::value<long double>(&args.lbound)->default_value(0.0),
			"minimum number (ldouble) to be generated")
		("ubound,u", po::value<long double>(&args.ubound)->default_value(1.0),
			"maximum number (ldouble) to be generated")
		("ceil,c", po::bool_switch(&args.ceil)->default_value(false),
			"apply ceiling function to numbers")
		("floor,f", po::bool_switch(&args.floor)->default_value(false),
			"apply floor function to numbers")
		("round,r", po::bool_switch(&args.round)->default_value(false),
			"apply round function to numbers")
		("trunc,t", po::bool_switch(&args.trunc)->default_value(false),
			"apply truncation to numbers")
		("precision,p", po::value<int>(&args.precision)->default_value(ld_prec), 
			"output precision (not internal precision, cannot be > ldouble precision)")
		("exclude,x", po::value<std::vector<long double> >(&args.excluded)->multitoken(), 
			"exclude numbers from being printed, best with --ceil, --floor, --round, or --trunc")
		("norepeat", po::bool_switch(&args.norepeat)->default_value(false), 
			"exclude repeated numbers from being printed, best with --ceil, --floor, --round, or --trunc")
		("stat-min", po::bool_switch(&args.stat_min)->default_value(false), 
			"print the lowest value generated")
		("stat-max", po::bool_switch(&args.stat_max)->default_value(false),
			"print the highest value generated")
		("stat-median", po::bool_switch(&args.stat_median)->default_value(false),
			"print the median of the values generated")
		("stat-avg", po::bool_switch(&args.stat_avg)->default_value(false),
			"print the average of the values generated")
		("prefix", po::value<std::vector<std::string> >(&args.prefix)->multitoken(),
			"only print when the number begins with string(s)")
		("suffix", po::value<std::vector<std::string> >(&args.suffix)->multitoken(),
			"only print when the number ends with string(s)")
		("contains", po::value<std::vector<std::string> >(&args.contains)->multitoken(),
			"only print when the number contains string(s)")
		("list", po::bool_switch(&args.list)->default_value(false),
			"print numbers in a list with positional numbers prefixed")
		("delim", po::value<std::string>(&args.delim),
			"change the delimiter")
		("quiet,q", po::bool_switch(&args.quiet)->default_value(false),
			"disable number output, useful when paired with stats")
		("numbers-force", po::bool_switch(&args.numbers_force)->default_value(false),
			"force the count of numbers output to be equal to the number specified")
		("generator,g", po::value<std::string>(&args.generator),
			"change algorithm for the RNG:\n - minstd_rand0\n - minstd_rand"
			"\n - mt19937 (default)\n - mt19937_64\n - ranlux24_base\n - ranlux48_base"
			"\n - ranlux24\n - ranlux48\n - knuth_b\n - default_random_engine"
			"\n - badrandom (std::rand)")
		("flags", po::bool_switch(&args.flags)->default_value(false),
			"print the flags");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if(vm.count("help")) {
		std::cout << desc << '\n';
		return returnID::success_help;
	}
	if(args.number <= 0) {
		std::cerr << "error: the argument for option '--number' is invalid (n must be >= 1)\n";
		return returnID::zero_err;
	}
	if(args.ceil + args.floor + args.round + args.trunc > 1) {
		std::cerr << "error: --ceil, --floor, --round, and --trunc are mutually exclusive\n";
		return returnID::conflict_err;
	}
	if(args.ceil || args.floor || args.round || args.trunc) {
		args.precision = 0;
	}
	if(args.precision > ld_prec) {
		std::cerr << "error: --precision cannot be greater than the precision for <long double> ("
			<< ld_prec << ")\n";
		return returnID::overd_err;
	}
	if(args.precision <= -1) {
		std::cerr << "error: --precision cannot be less than zero\n";
		return returnID::underd_err;
	}
	if(vm.count("exclude") && vm["exclude"].empty()) {
		std::cerr << "error: --exclude was specified without arguments (arguments are separated by spaces)\n";
		return returnID::exclude_err;
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

	const std::vector<std::string> gen_options {{"minstd_rand0", "minstd_rand", "mt19937",
		"mt19937_64", "ranlux24_base", "ranlux48_base", "ranlux24",
		"ranlux48", "knuth_b", "default_random_engine", "badrandom"}};
	if(std::find(gen_options.begin(), gen_options.end(), args.generator) == gen_options.end()) {
		std::cerr << "error: --generator must be: minstd_rand0, minstd_rand, "
			"mt19937, mt19937_64, ranlux24_base, ranlux48_base, "
			"ranlux24, ranlux48, knuth_b, default_random_engine, badrandom\n";
		return returnID::gen_err;
	}
	
	return returnID::success;
}

template<typename GEN> std::function<long double()> r_gen(const program_args & args) {
	GEN generator{std::random_device{}()};
	std::uniform_real_distribution<long double> dis{args.lbound, args.ubound};
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

int main(int ac, char* av[]) {
	try {
		program_args args;

		switch(auto result = parse_args(args, ac, av)) {
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
				if(args.list && args.numbers_force) std::cout << i << ".\t";
				if(args.list) std::cout << list_cnt << ".\t";
				std::cout << std::fixed << rand << args.delim;
				if(args.numbers_force) ++i;
			}
		}

		if(args.delim != "\n" && !args.quiet) std::cout << '\n';

		if((args.stat_min || args.stat_max || args.stat_median || args.stat_avg) && !args.quiet)
			std::cout << '\n';

		if(args.stat_min || args.stat_max) {
			auto minmax = std::minmax_element(generated.begin(), generated.end());
			if(args.stat_min) std::cout << "min: " << *minmax.first << '\n';
			if(args.stat_max) std::cout << "max: " << *minmax.second << '\n';
		}

		if(args.stat_median) {
			auto midpoint = generated.begin() + generated.size() / 2;
			std::nth_element(generated.begin(), midpoint, generated.end());
			auto median = *midpoint;
			if(generated.size() % 2 == 0)
				median = (median + *std::max_element(generated.begin(), midpoint)) / 2;
			std::cout << "median: " << median << '\n';
		}
		if(args.stat_avg) {
			long double sum = std::accumulate(generated.begin(), generated.end(), 0.0);
			std::cout << "avg: " << sum / generated.size() << '\n';
		}

		if(args.flags) {
			std::cout << "\nhelp,h: " << "0"
				<< "\nnumber,n: " << args.number
				<< "\nlbound,l: " << args.lbound
				<< "\nubound,u: " << args.ubound
				<< "\nceil,c: " << args.ceil
				<< "\nfloor,f: " << args.floor
				<< "\nround,r: " << args.round
				<< "\ntrunc,t: " << args.trunc
				<< "\nprecision,p: " << args.precision
				<< "\nexclude,x: ";
			for(const auto & i : args.excluded) std::cout << i << ' ';
			std::cout << "\nnorepeat: " << args.norepeat
				<< "\nstat-min: " << args.stat_min
				<< "\nstat-max: " << args.stat_max
				<< "\nstat-median: " << args.stat_median
				<< "\nstat-avg: " << args.stat_avg
				<< "\nprefix: ";
			for(const auto & i : args.prefix) std::cout << i << ' ';
			std::cout << "\nsuffix: ";
			for(const auto & i : args.suffix) std::cout << i << ' ';
			std::cout << "\ncontains: ";
			for(const auto & i : args.contains) std::cout << i << ' ';
			std::cout << "\nlist: " << args.list
				<< "\ndelim: " << args.delim
				<< "\nquiet,q: " << args.quiet
				<< "\nnumbers-force: " << args.numbers_force
				<< "\ngenerator,g: " << args.generator
				<< "\nflags: " << args.flags;
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
