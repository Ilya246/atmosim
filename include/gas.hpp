#include <string>

struct gas_type {
    float specific_heat;
    std::string name;
};

struct gas_inst {
    float amount;
    const gas_type& type;
};
