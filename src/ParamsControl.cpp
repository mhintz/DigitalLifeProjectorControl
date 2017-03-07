#include "ParamsControl.h"

using namespace ci;
using std::string;

vec3 parseVector(JsonTree vec) {
	return vec3(vec.getValueAtIndex<float>(0), vec.getValueAtIndex<float>(1), vec.getValueAtIndex<float>(2));
}

Color parseColor(JsonTree color) {
	return Color(color.getValueAtIndex<float>(0), color.getValueAtIndex<float>(1), color.getValueAtIndex<float>(2));
}

Projector parseProjectorParams(JsonTree const & params) {
	return Projector()
		.setId(params.getValueForKey<int>("id"))
		.setHorFOV(params.getValueForKey<float>("horFOV"))
		.setVertFOV(params.getValueForKey<float>("vertFOV"))
		.setVertBaseAngle(params.getValueForKey<float>("baseAngle"))
		.moveTo(parseVector(params.getChild("position")))
		.setUpsideDown(params.getValueForKey<bool>("isUpsideDown"))
		.setYRotation(params.getValueForKey<float>("yRotation"))
		.setColor(parseColor(params.getChild("color")));
}

JsonTree loadProjectorParams(app::App * theApp, string paramFileName) {
	try {
		return JsonTree(theApp->loadAsset(paramFileName));
	} catch (app::AssetLoadExc exc) {
		app::console() << "Failed to load parameters - they probably don't exist yet : " << exc.what() << std::endl;
	} catch (app::ResourceLoadExc exc) {
		app::console() << "Failed to load parameters - they probably don't exist yet : " << exc.what() << std::endl;
	} catch (JsonTree::ExcChildNotFound exc) {
		app::console() << "Failed to load one of the JsonTree children: " << exc.what() << std::endl;
	}
	return JsonTree();
}

JsonTree serializeVector(string name, vec3 vec) {
	return JsonTree::makeArray(name)
		.addChild(JsonTree("", vec.x))
		.addChild(JsonTree("", vec.y))
		.addChild(JsonTree("", vec.z));
}

JsonTree serializeColor(string name, Color color) {
	return JsonTree::makeArray(name)
		.addChild(JsonTree("", color.r))
		.addChild(JsonTree("", color.g))
		.addChild(JsonTree("", color.b));
}

JsonTree serializeProjector(Projector const & proj) {
	return JsonTree()
		.addChild(JsonTree("id", proj.getId()))
		.addChild(JsonTree("horFOV", proj.getHorFOV()))
		.addChild(JsonTree("vertFOV", proj.getVertFOV()))
		.addChild(JsonTree("baseAngle", proj.getVertBaseAngle()))
		.addChild(serializeVector("position", proj.getPos()))
		.addChild(JsonTree("isUpsideDown", proj.getUpsideDown()))
		.addChild(JsonTree("yRotation", proj.getYRotation()))
		.addChild(serializeColor("color", proj.getColor()));
}

void saveProjectorParams(app::App * theApp, std::vector<ProjectorRef> const & theData, string paramFileName) {
	JsonTree appParams;

	for (auto & proj : theData) {
		appParams.addChild(serializeProjector(* proj));
	}

	string serializedParams = appParams.serialize();
	std::ofstream writeFile;

	// Write to local file
	string appOwnFile = theApp->getAssetPath(paramFileName).string();
	writeFile.open(appOwnFile);
	std::cout << "writing params to: " << appOwnFile << std::endl;
	writeFile << serializedParams;
	writeFile.close();

	// This code will work if you're developing in the repo.
	// Otherwise it throws and the parameters aren't written to the local version
	// of the parameters resource file. This is a really ugly way to do things,
	// sometime when I'm feeling up to it, I'll dig deeper into the boost filesystem code
	// and come up with a better way of handling this.
	// try {
	// 	fs::path repoFolder = fs::canonical("../../../assets");
	// 	if (fs::is_directory(repoFolder)) {
	// 		string repoFile = fs::canonical(repoFolder.string() + "/" + paramFileName).string();
	// 		writeFile.open(repoFile);
	// 		std::cout << "writing params to: " << repoFile << std::endl;
	// 		writeFile << serializedParams;
	// 		writeFile.close();
	// 	}
	// } catch (fs::filesystem_error exp) {
	// 	app::console() << "Encountered an error while reading from a file: " << exp.what() << std::endl;
	// }
}
