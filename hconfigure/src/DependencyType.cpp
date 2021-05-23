

#include <DependencyType.hpp>

void to_json(Json &j, const DependencyType &p) {
  if (p == DependencyType::PUBLIC) {
    j = "PUBLIC";
  } else if (p == DependencyType::PRIVATE) {
    j = "PRIVATE";
  } else {
    j = "INTERFACE";
  }
}
