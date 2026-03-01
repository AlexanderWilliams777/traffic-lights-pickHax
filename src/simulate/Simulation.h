#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

// Forward declarations
struct Node;
struct Edge;
struct Car;
struct TrafficLight;
struct TrafficController;
struct Metrics;

// Strategy interface (abstract base used by TrafficController)
struct TrafficStrategy {
    virtual ~TrafficStrategy() = default;
    // Returns a PhaseMask (uint32_t) where each bit corresponds to an allowed movement
    virtual uint32_t choosePhase(class Simulation& sim, class TrafficController& controller) = 0;
};

//////////////////////////////////////////////////////////////
// Basic Math
//////////////////////////////////////////////////////////////

struct Vec2 {
    float x;
    float y;
};

//////////////////////////////////////////////////////////////
// Road Network
//////////////////////////////////////////////////////////////

struct Node {
    int id;
    Vec2 position;
};

struct Edge {
    int id;

    Node* start;
    Node* end;

    float length;
    float speedLimit;

    std::vector<int> carsOnEdge; // indices into Simulation::cars
    float heat = 0.0f;
    int capacity = 0;
    int occupancy = 0;
};

struct RoadNetwork {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

//////////////////////////////////////////////////////////////
// Traffic System
//////////////////////////////////////////////////////////////

enum class LightState {
    Red,
    Straight,
    Left,
    Right
};

enum class CarTurn {
    Straight,
    Left,
    Right
};

struct TrafficLight {

    int id;

    Edge* controlledEdge;

    LightState state;

    float timer;
    // minimum green time this light must keep allowed movements
    float minGreen = 1.5f;

    float straightDuration;
    float leftDuration;
    float rightDuration;
    float redDuration;
    int intersectionId = -1; // node id of the intersection this light controls (end node)
    // allowed movement flags
    bool allowStraight = true;
    bool allowLeft = false;
    bool allowRight = false;
};

struct TrafficController {
    // traffic lights (legacy, kept for compatibility)
    std::vector<TrafficLight> lights;

    // Movement primitives
    struct Movement {
        Edge* incoming = nullptr;
        Edge* outgoing = nullptr;
        int id = -1;
    };

    std::vector<Movement> movements;
    // conflict matrix: movements x movements
    std::vector<std::vector<bool>> conflictMatrix;

    using PhaseMask = uint32_t;
    std::vector<PhaseMask> validPhases;
    PhaseMask currentPhase = 0;

    // timing
    float phaseTimer = 0.0f;
    float minGreenTime = 1.0f;
    float maxGreenTime = 10.0f; // 0 = no max

    // strategy interface (owning)
    std::unique_ptr<TrafficStrategy> strategy;

    // choose/update current phase according to strategy
    void update(class Simulation &sim, float dt);
    void setStrategy(std::unique_ptr<TrafficStrategy> s) { strategy = std::move(s); }
};

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Vehicles
//////////////////////////////////////////////////////////////

struct Car {
    int id;
    Edge* currentEdge;
    float s;
    float velocity;
    float acceleration;
    float desiredSpeed;
    float length;
    bool waitingAtLight;
    CarTurn desiredTurn = CarTurn::Straight;
    // lane index (0 = left, 1 = right)
    int lane = 1;
    // cooldown to avoid rapid lane changes
    float laneChangeCooldown = 0.0f;
    // for metrics
    float totalWait = 0.0f;
    float spawnTime = 0.0f;
};

//////////////////////////////////////////////////////////////
// Metrics (For Optimization & Analysis)
//////////////////////////////////////////////////////////////

struct Metrics {

    float totalWaitTime = 0.0f;
    float cumulativeSpeed = 0.0f;
    int carsPassed = 0;
    // additional metrics
    int totalCarsExited = 0;
    float maxWaitTime = 0.0f;
    int throughput = 0;
    float totalSystemTime = 0.0f;

    void reset() {
        totalWaitTime = 0.0f;
        cumulativeSpeed = 0.0f;
        carsPassed = 0;
    }
};

//////////////////////////////////////////////////////////////
// Simulation Core
//////////////////////////////////////////////////////////////

class Simulation {
public:

    Simulation(float timestep);

    // visualization-only fluid-like heatmap structure
    struct HeatMap {
        int width = 0;
        int height = 0;
        float cellSize = 1.0f;
        float originX = 0.0f;
        float originY = 0.0f;

        float decayRate = 0.95f;
        float diffusionRate = 0.2f;

        std::vector<float> cells;
        std::vector<float> buffer;
    };

    // Turn probabilities (can be modified by UI)
    float probStraight = 0.6f;
    float probLeft = 0.2f;
    float probRight = 0.2f;
    // simulation time scale multiplier (1.0 = real-time)
    float timeScale = 1.0f;

    void initializeIntersection();
    // advance simulation by frame delta time (seconds)
    void update(float frameDt);
    void reset();
    // Create N cars immediately (used by UI). If edgeIndex >= 0, spawn on that edge; otherwise random.
    void createCars(int count, int edgeIndex = -1);
    // Manually set allowed movements for a light controlling the given edge index (-1 = none)
    void setLightAllowsForEdge(int edgeIndex, bool allowStraight, bool allowLeft, bool allowRight);
    // If true, controller won't override manual light settings
    bool manualLightControl = false;

    // Accessors for rendering layer
    const RoadNetwork& getNetwork() const;
    const std::vector<Car>& getCars() const;
    const TrafficController& getController() const;
    const Metrics& getMetrics() const;
    // expose heatmap for visualization
    const HeatMap& getHeatMap() const { return heatMap; }
    // allow setting controller strategy by index (0=Fixed,1=Density,2=Queue,3=Heat,4=Predictive,5=Hybrid)
    void setControllerStrategyByIndex(int idx);
    // Scoring helpers for traffic strategies (public so strategies can call them)
    float scoreMovementDensity(int movementId) const;
    float scoreMovementQueue(int movementId, float threshold) const;
    float scoreMovementPredict(int movementId, float horizon) const;
    float scoreMovementHeat(int movementId) const;

private:

    // Core State
    RoadNetwork network;
    std::vector<Car> cars;
    TrafficController controller;
    Metrics metrics;

    // Time
    float time = 0.0f;
    float dt;

    // Internal Helpers
    void updateCars(float dt);
    void updateCar(Car& car, float dt);
    // heatMap member (defined public HeatMap type above)
    HeatMap heatMap;

    // resolve overlaps after motion
    void resolveOverlaps();
    // ensure edge car lists are consistent and sorted by s
    void normalizeEdgeCarLists();

    // congestion heat update
    void updateHeat();
    // grid-based visual heatmap (visualization only)
    void updateHeatMap();

    void handleEdgeTransitions();
    void spawnCars();

    Car* getCarAhead(const Car& car);
    // Ray-based detection for any car ahead along the path (returns max allowed s before collision)
    Car* rayCarAhead(const Car& car, float &outMaxS, float lookahead = 50.0f) const;
    bool approachingRedLight(const Car& car) const;

    float computeGap(const Car& a, const Car& b) const;
};