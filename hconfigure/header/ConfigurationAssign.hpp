#ifndef HMAKE_CONFIGURATION_ASSIGN_HPP
#define HMAKE_CONFIGURATION_ASSIGN_HPP

#ifdef USE_HEADER_UNITS
import "CppSourceTarget.hpp";
#else
#include "CppSourceTarget.hpp"
#endif

template <typename T, typename... Property> Configuration &Configuration::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        targetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ConfigType>)
    {
        compilerFeatures.setConfigType(property);
        linkerFeatures.setConfigType(property);
    }
    else if constexpr (std::is_same_v<decltype(property), DSC<CppSourceTarget> *>)
    {
        stdCppTarget = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AssignStandardCppTarget>)
    {
        assignStandardCppTarget = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTests>)
    {
        buildTests = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildExamples>)
    {
        buildExamples = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TestsExplicit>)
    {
        testsExplicit = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExamplesExplicit>)
    {
        examplesExplicit = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTestsExplicitBuild>)
    {
        if (property == BuildTestsExplicitBuild::YES)
        {
            buildTests = BuildTests::YES;
            testsExplicit = TestsExplicit::YES;
        }
        else
        {
            buildTests = BuildTests::NO;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), BuildExamplesExplicitBuild>)
    {
        if (property == BuildExamplesExplicitBuild::YES)
        {
            buildExamples = BuildExamples::YES;
            examplesExplicit = ExamplesExplicit::YES;
        }
        else
        {
            buildExamples = BuildExamples::NO;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTestsAndExamples>)
    {
        if (property == BuildTestsAndExamples::YES)
        {
            buildTests = BuildTests::YES;
            buildExamples = BuildExamples::YES;
        }
        else
        {
            buildTests = BuildTests::NO;
            buildExamples = BuildExamples::NO;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTestsAndExamplesExplicitBuild>)
    {
        if (property == BuildTestsAndExamplesExplicitBuild::YES)
        {
            buildTests = BuildTests::YES;
            buildExamples = BuildExamples::YES;
            testsExplicit = TestsExplicit::YES;
            examplesExplicit = ExamplesExplicit::YES;
        }
        else
        {
            buildTests = BuildTests::NO;
            buildExamples = BuildExamples::NO;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), TreatModuleAsSource>)
    {
        treatModuleASSource = property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdAsHeaderUnit>)
    {
        stdAsHeaderUnit = property;
    }
    // CommonFeatures
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        compilerFeatures.targetOs = property;
        linkerFeatures.targetOs = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        compilerFeatures.debugSymbols = property;
        linkerFeatures.debugSymbols = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        compilerFeatures.profiling = property;
        linkerFeatures.profiling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Visibility>)
    {
        compilerFeatures.localVisibility = property;
        linkerFeatures.visibility = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        compilerFeatures.addressSanitizer = property;
        linkerFeatures.addressSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        compilerFeatures.leakSanitizer = property;
        linkerFeatures.leakSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        compilerFeatures.threadSanitizer = property;
        linkerFeatures.threadSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        compilerFeatures.undefinedSanitizer = property;
        linkerFeatures.undefinedSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        compilerFeatures.coverage = property;
        linkerFeatures.coverage = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        compilerFeatures.lto = property;
        linkerFeatures.lto = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        compilerFeatures.ltoMode = property;
        linkerFeatures.ltoMode = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        compilerFeatures.runtimeLink = property;
        linkerFeatures.runtimeLink = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        compilerFeatures.arch = property;
        linkerFeatures.arch = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        compilerFeatures.addModel = property;
        linkerFeatures.addModel = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        compilerFeatures.debugStore = property;
        linkerFeatures.debugStore = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        compilerFeatures.runtimeDebugging = property;
        linkerFeatures.runtimeDebugging = property;
    }
    // CppCompilerFeatures
    else if constexpr (std::is_same_v<decltype(property), Compiler>)
    {
        compilerFeatures.compiler = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        compilerFeatures.threading = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        compilerFeatures.cxxStd = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        compilerFeatures.cxxStdDialect = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Optimization>)
    {
        compilerFeatures.optimization = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Inlining>)
    {
        compilerFeatures.inlining = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        compilerFeatures.warnings = property;
    }
    else if constexpr (std::is_same_v<decltype(property), WarningsAsErrors>)
    {
        compilerFeatures.warningsAsErrors = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExceptionHandling>)
    {
        compilerFeatures.exceptionHandling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AsyncExceptions>)
    {
        compilerFeatures.asyncExceptions = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RTTI>)
    {
        compilerFeatures.rtti = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExternCNoThrow>)
    {
        compilerFeatures.externCNoThrow = property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        compilerFeatures.stdLib = property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        compilerFeatures.instructionSet = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        compilerFeatures.cpuType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        ploatFeatures.copyToExeDirOnNtOs = property;
    }
    // CppTargetFeatures
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        if (stdCppTarget && stdCppTarget->objectFileProducer)
        {
            stdCppTarget->getSourceTarget().reqCompileDefinitions.emplace(property);
        }
    }
    else if constexpr (std::is_same_v<decltype(property), CxxFlags>)
    {
        if (stdCppTarget && stdCppTarget->objectFileProducer)
        {
            stdCppTarget->getSourceTarget().reqCompilerFlags += property;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        if (stdCppTarget && stdCppTarget->objectFileProducer)
        {
            stdCppTarget->getSourceTarget().reqCompileDefinitions.emplace(property);
        }
    }

    // Linker Features
    else if constexpr (std::is_same_v<decltype(property), LinkFlags>)
    {
        if (stdCppTarget && stdCppTarget->ploat)
        {
            stdCppTarget->getLOAT().reqLinkerFlags += property;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        linkerFeatures.userInterface = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Strip>)
    {
        linkerFeatures.strip = property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        linkerFeatures.strip = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
    if constexpr (sizeof...(properties))
    {
        return assign(properties...);
    }
    else
    {
        return *this;
    }
}

#endif // HMAKE_CONFIGURATION_ASSIGN_HPP
