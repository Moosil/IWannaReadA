#include <qapplication.h>
#include <spdlog/spdlog.h>

#include "application.h"
#include "ocr_engine.h"
#include "screenshot.h"
#include "tooltip.h"

using namespace iwra;

int main(int argc, char* argv[]) {
	#ifdef WIN32
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
	#endif

	// ReSharper disable once CppLocalVariableWithNonTrivialDtorIsNeverUsed
	Application app(argc, argv);

	return QApplication::exec();
}
