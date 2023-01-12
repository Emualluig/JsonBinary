#include <fstream>
#include <iostream>
#include "JsonBinaryCPP.h"

int main() {
	std::ifstream ifs("test.json");
	nlohmann::json json = nlohmann::json::parse(ifs);
	const auto bytes = JsonBinary::serialize(json);
	nlohmann::json deser = JsonBinary::deserialize(bytes);
	std::cout << json << "\n" << deser << "\n";
}
