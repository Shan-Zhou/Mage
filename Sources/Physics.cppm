/*
* Data-oriented designed physics engine
* Single module, hopefully single file if it's still readable
* Currently use Eigen3 for both linear algebra and vectorized (floating number) container
* Some parts might be moved to GPU
*/
module;
#pragma warning (push)
#pragma warning (disable: 4819) // Non-ANSI characters
#include "Eigen/Dense"
#pragma warning (pop)
#include <cassert>
#include <memory>
export module Physics;
//import <memory>;
//import :Types;

/*
* Type aliases, NOT exported
* The usage of Eigen is restricted in this module and all interfaces return (customized) span type
* Since unexported namesapce won't leak, using namespace xxx is safe
* All data buffers are owned by this Physics modules
*/
using namespace Eigen;
namespace Physics {
	using FloatType = float;
}

/*
* Data buffers, thin abstraction level
* Simulate interface of std::vector
* Currently the allocator is hard wired, might support customized one in the future
* Memory layout is "flattable", i.e., as if all "struct ... {" and "}" are deleted
*/
export namespace Physics {
	struct BodyInfo {
		Vector3<FloatType> position;
		Vector3<FloatType> velocity;
		Vector3<FloatType> momentum;
	};
	struct BodyData {
		MatrixX3<FloatType> positions;
		MatrixX3<FloatType> velocities;
		MatrixX3<FloatType> momenta;
		Eigen::Index size;
		Eigen::Index capacity;
		void Reallocate(Eigen::Index new_capacity) {
			velocities.resize(new_capacity, 3);
			momenta.resize(new_capacity, 3);
			capacity = new_capacity;
			assert(size <= capacity);
		}
		void AddBody(const BodyInfo& body) {
			if (size+1 > capacity) {
				Reallocate(capacity * 2);
			}
			positions(size, all) = body.position;
			++size;
			// add body
		}
	};
	struct PhysicsData {
		BodyData body_data;
		bool bodydata_enabled;
	};
}

/*
* Data manipulations
* One should have some pipelines in mind when reading codes of this part
*/
export namespace Physics {
	void integrate_kinematics(const PhysicsData& physics_data);
	void collide();
	void simulate();
}

/*
* Configure Eigen library
*/
export namespace Physics {
	void SetNumOfThreads(int num) {
		Eigen::setNbThreads(num);
	}

	int NumOfThreads() {
		return Eigen::nbThreads();
	}
}

export namespace Physics {
	struct Particle {
		Particle();
	};
}

/*
* Implementation details
*/
module :private;
namespace Physics {
	Particle::Particle() {
		FloatType a;
		//Vector3 ab{ a, a, a + 1 };
		//Eigen::Vector3<FloatType> ab;
	}

}