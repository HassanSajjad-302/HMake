

#include "PackageVariant.hpp"
#include "Project.hpp"

PackageVariant::PackageVariant() {
  packageVariantConfigurationType = Project::projectConfigurationType;
  packageVariantCompiler = Project::ourCompiler;
  packageVariantLinker = Project::ourLinker;
}
