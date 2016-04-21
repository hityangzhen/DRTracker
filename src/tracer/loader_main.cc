#include "tracer/loader.h"

int main(int argc,char *argv[])
{
	tracer::Loader *loader=new tracer::Loader;
	loader->Initialize();
	loader->PreSetup();
	loader->Parse(argc,argv);
	loader->PostSetup();
	loader->Start();
	loader->Exit();
	delete loader;
}