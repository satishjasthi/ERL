/*
ERL

Main
*/

#include <erl/ERLConfig.h>

#include <neat/Evolver.h>
#include <neat/NetworkGenotype.h>
#include <neat/NetworkPhenotype.h>
#include <neat/Evolver.h>

#include <erl/platform/Field2DGenesToCL.h>
#include <erl/visualization/FieldVisualizer.h>
#include <erl/simulation/EvolutionaryTrainer.h>
#include <erl/field/Field2DEvolverSettings.h>

#include <erl/experiments/ExperimentAND.h>
#include <erl/experiments/ExperimentOR.h>
#include <erl/experiments/ExperimentXOR.h>
#include <erl/experiments/ExperimentPoleBalancing.h>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <time.h>
#include <iostream>
#include <fstream>

// Sets the mode of execution
#define TRAIN_ERL

int main() {
	std::cout << "Welcome to ERL. Version " << ERL_VERSION << std::endl;

	erl::Logger logger;

	logger.createWithFile("erlLog.txt");

	erl::ComputeSystem cs;

	cs.create(erl::ComputeSystem::_gpu, logger);

	std::mt19937 generator(time(nullptr));

	std::vector<float> functionChances(3);
	std::vector<std::string> functionNames(3);
	std::vector<std::function<float(float)>> functions(3);
	neat::InnovationNumberType innovNum = 0;

	functionChances[0] = 1.0f;
	functionChances[1] = 1.0f;
	functionChances[2] = 1.0f;

	functionNames[0] = "sigmoid";
	functionNames[1] = "sin";
	functionNames[2] = "linear";

	functions[0] = std::bind(neat::Neuron::sigmoid, std::placeholders::_1);
	functions[1] = std::bind(std::sinf, std::placeholders::_1);
	functions[2] = std::bind([](float x) { return std::min<float>(2.0f, std::max<float>(-2.0f, x)); }, std::placeholders::_1);

	// Load random texture
	sf::Image sfmlImage;

	if (!sfmlImage.loadFromFile("random.bmp")) {
		logger << "Could not load random.bmp!" << erl::endl;
	}

	erl::SoftwareImage2D<sf::Color> softImage;

	softImage.reset(sfmlImage.getSize().x, sfmlImage.getSize().y);

	for (int x = 0; x < sfmlImage.getSize().x; x++)
	for (int y = 0; y < sfmlImage.getSize().y; y++) {
		softImage.setPixel(x, y, sfmlImage.getPixel(x, y));
	}

	std::shared_ptr<cl::Image2D> randomImage(new cl::Image2D(cs.getContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_RGBA, CL_UNORM_INT8), softImage.getWidth(), softImage.getHeight(), 0, softImage.getData()));

#ifdef TRAIN_ERL
	// ------------------------------------------- Training -------------------------------------------

	std::shared_ptr<neat::EvolverSettings> settings(new erl::Field2DEvolverSettings());

	erl::EvolutionaryTrainer trainer;

	trainer.create(functionChances, settings, randomImage, functions, functionNames, -1.0f, 1.0f, generator);

	trainer.addExperiment(std::shared_ptr<erl::Experiment>(new ExperimentXOR()));
	trainer.addExperiment(std::shared_ptr<erl::Experiment>(new ExperimentOR()));
	trainer.addExperiment(std::shared_ptr<erl::Experiment>(new ExperimentAND()));

	for (size_t g = 0; g < 10000; g++) {
		logger << "Evaluating generation " << std::to_string(g + 1) << "." << erl::endl;

		trainer.evaluate(cs, logger, generator);

		logger << "Reproducing generation " << std::to_string(g + 1) << "." << erl::endl;

		trainer.reproduce(generator);

		logger << "Saving best to \"hypernet1.txt\"" << erl::endl;

		std::ofstream toFile("erlBestResultSoFar.txt");

		trainer.writeBestToStream(toFile);

		toFile.close();

		logger << "Generation completed." << erl::endl;

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
			break;
	}

#else

	// ------------------------------------------- Testing -------------------------------------------

	erl::Field2DGenes genes;

	std::ifstream fromFile("erlBestResultSoFar.txt");

	genes.readFromStream(fromFile);

	fromFile.close();

	erl::Field2D field;

	float sizeScalar = 800.0f / 128.0f;

	field.create(genes, cs, 128, 128, 2, 2, 1, randomImage, functions, functionNames, -1.0f, 1.0f, generator, logger);

	field.setInput(0, 10.0f);
	field.setInput(1, 10.0f);

	sf::RenderWindow window;
	window.create(sf::VideoMode(800, 800), "ERL Test", sf::Style::Default);

	bool quit = false;

	sf::Clock clock;

	float dt = 0.017f;

	erl::FieldVisualizer fv;

	fv.create(cs, "adapter.cl", field, logger);

	do {
		clock.restart();

		// ----------------------------- Input -----------------------------

		sf::Event windowEvent;

		while (window.pollEvent(windowEvent))
		{
			switch (windowEvent.type)
			{
			case sf::Event::Closed:
				quit = true;
				break;
			}
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
			quit = true;

		// -------------------------------------------------------------------

		window.clear();

		field.update(0.0f, cs, functions, 1, generator);

		fv.update(cs, field);

		sf::Image image;
		image.create(fv.getSoftImage().getWidth(), fv.getSoftImage().getHeight());

		for (int x = 0; x < image.getSize().x; x++)
		for (int y = 0; y < image.getSize().y; y++) {
			image.setPixel(x, y, fv.getSoftImage().getPixel(x, y));
		}

		sf::Texture texture;
		texture.loadFromImage(image);

		sf::Sprite sprite;
		sprite.setTexture(texture);
		sprite.setScale(sf::Vector2f(sizeScalar, sizeScalar));

		window.draw(sprite);

		// -------------------------------------------------------------------

		window.display();

		dt = clock.getElapsedTime().asSeconds();
	} while (!quit);

#endif

	return 0;
}


