/* -----------------------------------------------------------------------
 *  <copyright file="Main.cpp" company="Global Graphics Software Ltd">
 *      Copyright (c) 2023 Global Graphics Software Ltd. All rights reserved.
 *  </copyright>
 *  <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations regarding the use or
 *  results of use of this example.
 *  </summary>
 * -----------------------------------------------------------------------
 */

#include <iostream>
#include <filesystem>

#include <jawsmako/jawsmako.h>
#include <edl/icolormanager.h>
#include <jawsmako/customtransform.h>

#include "cxxopts.hpp"

#include "CmykBlackConverter.h"

namespace fs = std::filesystem;

using namespace JawsMako;
using namespace EDL;

int main(int argc, char* argv[])
{
    try
    {
        // Deal with program options
        cxxopts::Options options("CmykBlackConverter", "Convert rich black to K-only black\n");
        options
            .positional_help("<input file> [<output file>]")
            .set_width(70)
            .add_options()
            ("infile", "Input file", cxxopts::value<std::string>())
            ("outfile", "Output file", cxxopts::value<std::string>()->default_value("*"))
            ("d,devicen", "Use a DeviceN (spot) colour black, instead of a DeviceCMYK black")
            ("o,overprint", "Do *not* set overprint on changed objects")
            ("h,help", "Show this Usage information");

        options.parse_positional({ "infile", "outfile" });

        const auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            return 0;
        }

        const U8String inputFile = result["infile"].as<std::string>().c_str();
        if (!fs::exists(inputFile))
            throw std::invalid_argument(std::string("Input file not found."));

        U8String outputFile = result["outfile"].as<std::string>().c_str();
        if (outputFile == "*")
            outputFile = (fs::path(inputFile).remove_filename().string() + fs::path(inputFile).stem().string() + "_out.pdf").c_str();

        const bool useDeviceN = result["devicen"].as<bool>();
        const bool doNotApplyOverprint = result["overprint"].as<bool>();

        // Create our JawsMako instance.
        const IJawsMakoPtr jawsMako = IJawsMako::create();
        IJawsMako::enablePDFInput(jawsMako);
        IJawsMako::enablePDFOutput(jawsMako);

        // Create our input
        const IInputPtr input = IInput::create(jawsMako, eFFPDF);
        const IOutputPtr output = IOutput::create(jawsMako, eFFPDF);

        const IDocumentAssemblyPtr assembly = input->open(inputFile);
        const IDocumentPtr document = assembly->getDocument();

        // Choose the color converter. This is a custom transform implementation, so
		// it needs to be wrapped in an ICustomTransform to be used.
        CCmykBlackConverterImplementation cmykBlackConverter(jawsMako, useDeviceN, doNotApplyOverprint);
        ICustomTransformPtr colorTransform = ICustomTransform::create(jawsMako, &cmykBlackConverter);

        for (uint32 pageIndex = 0; pageIndex < document->getNumPages(); pageIndex++)
        {
            // Get page
            IPagePtr page = document->getPage(pageIndex);
            colorTransform->transformPage(page);
        }

        output->writeAssembly(assembly, outputFile);
    }
    catch (IError& e)
    {
        const String errorFormatString = getEDLErrorString(e.getErrorCode());
        std::wcerr << L"Exception thrown: " << e.getErrorDescription(errorFormatString) << std::endl;
        return static_cast<int>(e.getErrorCode());
    }
    catch (cxxopts::exceptions::exception& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << "Run again with -h or --help to see the program usage information." << std::endl;
        return 1;
    }
    catch (std::exception& e)
    {
        std::wcerr << L"std::exception thrown: " << e.what() << std::endl;
        std::wcerr << L"Check your command line arguments." << std::endl;
        return 1;
    }

    return 0;
}
