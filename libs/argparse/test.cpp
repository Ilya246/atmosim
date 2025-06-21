#include "args.hpp"

#include <string>

int main(int argc, char** argv) {
    bool debug = false;
    std::string filename = "test";
    std::vector<std::string> values;
    std::tuple<std::string, int, bool> tup;
    std::tuple<std::string, std::tuple<int, std::string, bool>, bool, int> tuptup;
    std::vector<std::tuple<std::string, int, bool>> tupvec; // unsupported

    auto args = {
        argp::make_argument("debug", "d", "12345", debug),
        argp::make_argument("filename", "f", "test", filename),
        argp::make_argument("values", "v", "vector test", values),
        argp::make_argument("tup", "t", "tuple test", tup),
        argp::make_argument("tuptup", "tt", "tuple tuple test", tuptup),
        argp::make_argument("tupvec", "tv", "tuple vector test", tupvec)
    };
    argp::parse_arguments(args, argc, argv,
                          "Argparse test program\n"
                          "  Usage:\n"
                          "    run with args to test whether their values get properly set\n"
                          "    example: ./test --tup=[abc,3,f] --filename=debug",

                          "Post-help test test\n"
                          "  Test: test");

    std::cout << "debug " << debug << " default " << 0 << std::endl;

    std::cout << "filename " << filename << " default " << "test" << std::endl;

    std::cout << "values ";
    for (const std::string& s : values) std::cout << s << ",";
    std::cout << std::endl;

    std::cout << "tup " << std::get<0>(tup)
    << " " << std::get<1>(tup)
    << " " << std::get<2>(tup)
    << std::endl;

    std::cout << "tuptup " << std::get<0>(tuptup)
    << " " << std::get<0>(std::get<1>(tuptup))
    << " " << std::get<1>(std::get<1>(tuptup))
    << " " << std::get<2>(std::get<1>(tuptup))
    << " " << std::get<2>(tuptup)
    << " " << std::get<3>(tuptup) << std::endl;

    /* unsupported
    for (const auto& t : tupvec) {
        std::cout << "t " << std::get<0>(t) << " " << std::get<1>(t) << " " << std::get<2>(t) << std::endl;
    } */

    return 0;
}
