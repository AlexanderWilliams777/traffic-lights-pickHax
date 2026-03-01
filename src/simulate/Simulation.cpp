#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <chrono>
#include <functional>

Simulation::Simulation(float timestep)
    : dt(timestep)
{
}



void Simulation::updateHeatMap()
{
    if (heatMap.width <= 0 || heatMap.height <= 0) return;
    const int W = heatMap.width;
    const int H = heatMap.height;
    const float cs = heatMap.cellSize;
    const float ox = heatMap.originX;
    const float oy = heatMap.originY;

    // Step 1: decay existing heat
    const float decay = heatMap.decayRate;
    for (int i = 0; i < W * H; ++i) heatMap.cells[i] *= decay;

    // Precompute gaussian sigma
    const float sigma = 2.0f * cs;
    const float sigma2 = sigma * sigma;
    const float radius = 3.0f * sigma;
    const int rCells = static_cast<int>(std::ceil(radius / cs));

    // Step 2: accumulate contributions from each car
    for (const auto &c : cars) {
        if (!c.currentEdge) continue;
        // compute world position
        const Vec2 &p0 = c.currentEdge->start->position;
        const Vec2 &p1 = c.currentEdge->end->position;
        float t = (c.currentEdge->length > 1e-6f) ? (c.s / c.currentEdge->length) : 0.0f;
        float wx = p0.x + (p1.x - p0.x) * t;
        float wy = p0.y + (p1.y - p0.y) * t;

        // map to grid coords (floating)
        float gx = (wx - ox) / cs;
        float gy = (wy - oy) / cs;
        int cx = static_cast<int>(std::floor(gx));
        int cy = static_cast<int>(std::floor(gy));

        // iterate neighborhood
        int x0 = std::max(0, cx - rCells);
        int x1 = std::min(W - 1, cx + rCells);
        int y0 = std::max(0, cy - rCells);
        int y1 = std::min(H - 1, cy + rCells);

        // compute slowdown factor: how much the car is below its desired speed
        float slowdown = 0.0f;
        if (c.desiredSpeed > 0.0f) slowdown = std::max(0.0f, c.desiredSpeed - c.velocity);
        float relSlow = slowdown / std::max(0.1f, c.desiredSpeed);
        // waitingAtLight indicates the car is stopped/queued (red light or blocked)
        float stopBonus = c.waitingAtLight ? 1.0f : 0.0f;

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                // cell center world pos
                float cxw = ox + (x + 0.5f) * cs;
                float cyw = oy + (y + 0.5f) * cs;
                float dx = cxw - wx;
                float dy = cyw - wy;
                float d2 = dx*dx + dy*dy;
                if (d2 > radius*radius) continue;
                float contrib = std::exp(-d2 / sigma2);
                // amplify contribution when the car is slowing/stopped due to lights or traffic
                float multiplier = 1.0f + 2.0f * relSlow + stopBonus;
                heatMap.cells[y * W + x] += contrib * multiplier;
            }
        }
    }

    // Step 3: diffusion pass (simple 5-point stencil)
    const float diff = heatMap.diffusionRate;
    // leave borders unchanged (or weakly diffused)
    for (int y = 1; y < H-1; ++y) {
        int row = y * W;
        for (int x = 1; x < W-1; ++x) {
            int i = row + x;
            float c = heatMap.cells[i];
            float lap = heatMap.cells[i-1] + heatMap.cells[i+1] + heatMap.cells[i - W] + heatMap.cells[i + W] - 4.0f * c;
            heatMap.buffer[i] = c + diff * lap;
        }
    }
    // copy borders (no diffusion)
    for (int x = 0; x < W; ++x) { heatMap.buffer[x] = heatMap.cells[x]; heatMap.buffer[(H-1)*W + x] = heatMap.cells[(H-1)*W + x]; }
    for (int y = 0; y < H; ++y) { heatMap.buffer[y*W + 0] = heatMap.cells[y*W + 0]; heatMap.buffer[y*W + (W-1)] = heatMap.cells[y*W + (W-1)]; }

    // swap buffers
    heatMap.cells.swap(heatMap.buffer);
}

void Simulation::updateHeat() {
    const float queueWindow = 15.0f; // meters from edge end to consider queued vehicles
    for (auto &e : network.edges) {
        // compute density (cars per meter)
        float density = 0.0f;
        if (e.length > 1e-6f) density = static_cast<float>(e.carsOnEdge.size()) / e.length;

        // compute queue intensity: number of cars within queueWindow meters of the edge end
        int queueCount = 0;
        float threshold = e.length - queueWindow;
        for (int idx : e.carsOnEdge) {
            if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
            const Car &c = cars[idx];
            if (c.currentEdge != &e) continue;
            if (c.s >= threshold) ++queueCount;
        }

        float newHeat = density + (static_cast<float>(queueCount) * 0.1f);
        // temporal smoothing
        e.heat = 0.9f * e.heat + 0.1f * newHeat;
    }
}

Car* Simulation::rayCarAhead(const Car& car, float &outMaxS, float lookahead) const {
    outMaxS = std::numeric_limits<float>::infinity();
    if (!car.currentEdge) return nullptr;
    Edge* edge = car.currentEdge;
    // search all cars on this edge (both lanes) for the nearest car ahead
    Car* best = nullptr;
    float bestGap = std::numeric_limits<float>::infinity();
    for (int idx : edge->carsOnEdge) {
        if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
        const Car &oc = cars[idx];
        if (&oc == &car) continue;
        if (oc.s <= car.s) continue; // must be ahead
        float gap = oc.s - car.s - oc.length;
        if (gap < bestGap) { bestGap = gap; best = const_cast<Car*>(&oc); }
    }
    if (best) {
        float safety = 1.5f;
        // max allowed s on this edge before colliding with best car
        outMaxS = best->s - (best->length + car.length + safety);
        return best;
    }
    // Optionally look into outgoing edges if close to end (within lookahead)
    float distToEnd = edge->length - car.s;
    if (distToEnd <= lookahead) {
        // check immediate outgoing edges from the intersection
        for (const auto &e : network.edges) {
            if (e.start != edge->end) continue;
            // find nearest car on that outgoing edge (smallest s)
            float nearestS = std::numeric_limits<float>::infinity();
            Car* nearestCar = nullptr;
            for (int idx : e.carsOnEdge) {
                if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
                const Car &oc = cars[idx];
                if (oc.currentEdge != &e) continue;
                if (oc.s < nearestS) { nearestS = oc.s; nearestCar = const_cast<Car*>(&oc); }
            }
            if (nearestCar) {
                float safety = 1.5f;
                // compute equivalent maxS on current edge: position of that car measured from current edge
                float frontGlobal = edge->length + nearestCar->s;
                float desiredGap = nearestCar->length + car.length + safety;
                float maxS = frontGlobal - desiredGap;
                // convert into current-edge s coordinate
                if (maxS < outMaxS) outMaxS = maxS;
                return nearestCar;
            }
        }
    }
    return nullptr;
}

void Simulation::normalizeEdgeCarLists() {
    for (auto &e : network.edges) {
        std::vector<int> list;
        list.reserve(e.carsOnEdge.size());
        for (int idx : e.carsOnEdge) {
            if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
            if (cars[idx].currentEdge == &e) list.push_back(idx);
        }
        // sort by s descending (front first)
        std::sort(list.begin(), list.end(), [&](int a, int b){ return cars[a].s > cars[b].s; });
        e.carsOnEdge = std::move(list);
    }
}

void Simulation::resolveOverlaps() {
    // For each edge, per lane, ensure cars are spaced with desired gap
    for (auto &e : network.edges) {
        // collect cars per lane
        std::vector<int> lane0, lane1;
        for (int idx : e.carsOnEdge) {
            if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
            Car &c = cars[idx];
            if (c.currentEdge != &e) continue;
            if (c.lane == 0) lane0.push_back(idx); else lane1.push_back(idx);
        }
        auto fixLane = [&](std::vector<int> &laneVec) {
            if (laneVec.empty()) return;
            // sort by s descending (front to back)
            std::sort(laneVec.begin(), laneVec.end(), [&](int a, int b){ return cars[a].s > cars[b].s; });
            // perform multiple passes to propagate constraints
            const int passes = 3;
            for (int pass = 0; pass < passes; ++pass) {
                for (size_t i = 1; i < laneVec.size(); ++i) {
                    Car &front = cars[laneVec[i-1]];
                    Car &back = cars[laneVec[i]];
                    // be conservative: require front and back lengths plus safety gap
                    float safety = 1.5f;
                    float desiredGap = front.length + back.length + safety;
                    float maxBackS = front.s - desiredGap;
                    if (back.s > maxBackS) {
                        back.s = std::max(0.0f, maxBackS);
                        // ensure velocity does not exceed front
                        if (back.velocity > front.velocity) back.velocity = front.velocity;
                        // if front is stopped, also stop the back to prevent creeping
                        if (front.velocity < 0.1f) { back.velocity = 0.0f; back.waitingAtLight = true; }
                    }
                }
            }
        };
        fixLane(lane0);
        fixLane(lane1);
    }
}

void Simulation::setLightAllowsForEdge(int edgeIndex, bool allowStraight, bool allowLeft, bool allowRight) {
    if (edgeIndex < 0 || edgeIndex >= static_cast<int>(network.edges.size())) return;
    Edge* e = &network.edges[edgeIndex];
    for (auto &l : controller.lights) {
        if (l.controlledEdge == e) {
            l.allowStraight = allowStraight;
            l.allowLeft = allowLeft;
            l.allowRight = allowRight;
            // update state to reflect allowed directions: none => Red
            if (!l.allowStraight && !l.allowLeft && !l.allowRight) l.state = LightState::Red;
            else if (l.allowStraight) l.state = LightState::Straight;
            else if (l.allowLeft) l.state = LightState::Left;
            else if (l.allowRight) l.state = LightState::Right;
            return;
        }
    }
}

void Simulation::createCars(int count, int edgeIndex) {
    if (count <= 0) return;
    static std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    if (network.edges.empty()) return;

    std::uniform_int_distribution<int> edgeDist(0, static_cast<int>(network.edges.size()) - 1);

    int attempts = 0;
    while (count > 0 && attempts < 2000) {
        int ei = edgeIndex >= 0 && edgeIndex < static_cast<int>(network.edges.size()) ? edgeIndex : edgeDist(rng);
        Edge &e = network.edges[ei];
        // do not spawn if occupied near start
        bool occupied = false;
        for (int idx : e.carsOnEdge) {
            if (idx >= 0 && idx < static_cast<int>(cars.size())) {
                if (cars[idx].s < 1.0f) { occupied = true; break; }
            }
        }
        if (!occupied) {
            Car c;
            c.id = static_cast<int>(cars.size());
            c.currentEdge = &e;
            c.s = 0.0f;
            c.desiredSpeed = e.speedLimit;
            c.velocity = c.desiredSpeed * 0.5f;
            c.acceleration = 0.0f;
            c.length = 4.0f;
            c.waitingAtLight = false;
            c.desiredTurn = CarTurn::Straight;
            c.lane = 1;
            c.laneChangeCooldown = 0.0f;
            cars.push_back(c);
            e.carsOnEdge.push_back(static_cast<int>(cars.size() - 1));
            e.occupancy = static_cast<int>(e.carsOnEdge.size());
            --count;
        }
        ++attempts;
        // if a specific edge was requested and we failed many times, break
        if (edgeIndex >= 0 && attempts > 1000) break;
    }
}

void Simulation::initializeIntersection() {
    // Build an NxN grid of nodes and connect them with bidirectional edges.
    // For gridN=5 this produces 16 intersections (4x4 cells) and more roads.
    network.nodes.clear();
    network.edges.clear();
    controller.lights.clear();
    cars.clear();
    metrics.reset();
    time = 0.0f;

    const int gridN = 5; // nodes per side (gridN-1 x gridN-1 intersections -> 4x4 = 16)
    const float spacing = 90.0f;
    // create gridN x gridN nodes (row-major: y*gridN + x)
    for (int y = 0; y < gridN; ++y) {
        for (int x = 0; x < gridN; ++x) {
            int id = y * gridN + x;
            network.nodes.push_back({id, {x * spacing, y * spacing}});
        }
    }

    // create bidirectional edges between orthogonal neighbors
    int edgeId = 0;
    auto nodeAt = [&](int x, int y) -> Node* {
        if (x < 0 || x >= gridN || y < 0 || y >= gridN) return nullptr;
        return &network.nodes[y * gridN + x];
    };

    for (int y = 0; y < gridN; ++y) {
        for (int x = 0; x < gridN; ++x) {
            Node* n = nodeAt(x, y);
            // right neighbor
            Node* rn = nodeAt(x + 1, y);
            if (rn) {
                Edge e1; e1.id = edgeId++; e1.start = n; e1.end = rn;
                float dx = e1.end->position.x - e1.start->position.x;
                float dy = e1.end->position.y - e1.start->position.y;
                e1.length = std::sqrt(dx*dx + dy*dy);
                e1.speedLimit = 25.0f; e1.carsOnEdge.clear(); network.edges.push_back(e1);
                Edge e2; e2.id = edgeId++; e2.start = rn; e2.end = n;
                e2.length = e1.length; e2.speedLimit = e1.speedLimit; e2.carsOnEdge.clear(); network.edges.push_back(e2);
            }
            // down neighbor
            Node* dn = nodeAt(x, y + 1);
            if (dn) {
                Edge e1; e1.id = edgeId++; e1.start = n; e1.end = dn;
                float dx = e1.end->position.x - e1.start->position.x;
                float dy = e1.end->position.y - e1.start->position.y;
                e1.length = std::sqrt(dx*dx + dy*dy);
                e1.speedLimit = 25.0f; e1.carsOnEdge.clear(); network.edges.push_back(e1);
                Edge e2; e2.id = edgeId++; e2.start = dn; e2.end = n;
                e2.length = e1.length; e2.speedLimit = e1.speedLimit; e2.carsOnEdge.clear(); network.edges.push_back(e2);
            }
        }
    }

    // Create traffic lights that control edges which end at the 4 interior intersections
    // Create traffic lights for edges that end at interior nodes (not on the border)
    for (size_t ei = 0; ei < network.edges.size(); ++ei) {
        Edge &e = network.edges[ei];
        int endIdx = e.end->id;
        int ex = endIdx % gridN;
        int ey = endIdx / gridN;
        if (ex > 0 && ex < gridN-1 && ey > 0 && ey < gridN-1) {
            TrafficLight tl;
            tl.id = static_cast<int>(controller.lights.size());
            tl.controlledEdge = &network.edges[ei];
            tl.intersectionId = e.end->id;
            tl.state = LightState::Straight;
            tl.allowStraight = true;
            tl.allowLeft = false;
            tl.allowRight = false;
            // require at least 2 seconds green by default so a few cars can pass
            tl.minGreen = 2.0f;
            tl.timer = 0.0f;
            tl.straightDuration = 4.0f;
            tl.leftDuration = 1.0f;
            tl.rightDuration = 1.0f;
            tl.redDuration = 3.0f;
            controller.lights.push_back(tl);
        }
    }

    // Build movements (incoming->outgoing) for each intersection
    controller.movements.clear();
    for (auto &node : network.nodes) {
        // collect incoming and outgoing edges for this node
        std::vector<Edge*> incoming, outgoing;
        for (auto &e : network.edges) {
            if (e.end == &node) incoming.push_back(&e);
            if (e.start == &node) outgoing.push_back(&e);
        }
        // create movements (exclude u-turns where outgoing->end == incoming->start)
        for (Edge* in : incoming) {
            for (Edge* out : outgoing) {
                if (!in || !out) continue;
                if (out->end == in->start) continue; // exclude u-turn
                TrafficController::Movement m;
                m.incoming = in;
                m.outgoing = out;
                m.id = static_cast<int>(controller.movements.size());
                controller.movements.push_back(m);
            }
        }
    }

    // initialize edge capacities/occupancy
    for (auto &e : network.edges) {
        e.occupancy = 0;
        e.capacity = std::max(1, static_cast<int>(std::floor(e.length / 6.0f)));
    }

    // build conflict matrix
    int M = static_cast<int>(controller.movements.size());
    controller.conflictMatrix.assign(M, std::vector<bool>(M, false));
    // helper to compute a short segment through the intersection for movement
    auto segFor = [&](const TrafficController::Movement &mv, Vec2 &a, Vec2 &b){
        Vec2 inStart = mv.incoming->start->position;
        Vec2 inEnd = mv.incoming->end->position;
        Vec2 outStart = mv.outgoing->start->position;
        Vec2 outEnd = mv.outgoing->end->position;
        // directions
        float inDx = inEnd.x - inStart.x; float inDy = inEnd.y - inStart.y;
        float inLen = std::sqrt(inDx*inDx + inDy*inDy);
        if (inLen > 1e-6f) { inDx /= inLen; inDy /= inLen; }
        float outDx = outEnd.x - outStart.x; float outDy = outEnd.y - outStart.y;
        float outLen = std::sqrt(outDx*outDx + outDy*outDy);
        if (outLen > 1e-6f) { outDx /= outLen; outDy /= outLen; }
        float offset = 1.0f;
        a.x = inEnd.x - inDx * offset; a.y = inEnd.y - inDy * offset;
        b.x = outStart.x + outDx * offset; b.y = outStart.y + outDy * offset;
    };

    auto segsIntersect = [&](const Vec2 &a1, const Vec2 &a2, const Vec2 &b1, const Vec2 &b2)->bool {
        auto orient = [](const Vec2 &p, const Vec2 &q, const Vec2 &r){
            return (q.x - p.x)*(r.y - p.y) - (q.y - p.y)*(r.x - p.x);
        };
        float o1 = orient(a1,a2,b1);
        float o2 = orient(a1,a2,b2);
        float o3 = orient(b1,b2,a1);
        float o4 = orient(b1,b2,a2);
        if (o1 == 0 && o2 == 0 && o3 == 0 && o4 == 0) {
            // collinear: treat as conflict
            return true;
        }
        return (o1*o2 < 0.0f) && (o3*o4 < 0.0f);
    };

    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < M; ++j) {
            if (i == j) { controller.conflictMatrix[i][j] = true; continue; }
            Vec2 a1,a2,b1,b2;
            segFor(controller.movements[i], a1, a2);
            segFor(controller.movements[j], b1, b2);
            if (segsIntersect(a1,a2,b1,b2)) controller.conflictMatrix[i][j] = true;
        }
    }

    // generate valid phases (bitmask of movements)
    controller.validPhases.clear();
    // If movement count is small, enumerate all masks; otherwise generate a deterministic
    // set of safe phases (single movements + greedy combinations) to avoid 2^M explosion.
    const int MAX_ENUMERATE = 16; // safe full enumeration limit
    int usableM = std::min(M, 32); // PhaseMask is 32 bits
    if (usableM <= MAX_ENUMERATE) {
        uint32_t maxMask = (usableM >= 32) ? 0u : (1u << usableM);
        for (uint32_t mask = 1u; mask < maxMask; ++mask) {
            bool ok = true;
            for (int i = 0; i < usableM && ok; ++i) {
                if (!(mask & (1u<<i))) continue;
                for (int j = i+1; j < usableM; ++j) {
                    if (!(mask & (1u<<j))) continue;
                    if (controller.conflictMatrix[i][j]) { ok = false; break; }
                }
            }
            if (ok) controller.validPhases.push_back(static_cast<TrafficController::PhaseMask>(mask));
        }
    } else {
        // large movement sets: include all single-movement phases
        for (int i = 0; i < usableM; ++i) controller.validPhases.push_back(static_cast<TrafficController::PhaseMask>(1u << i));
        // greedy forward combination
        for (int i = 0; i < usableM; ++i) {
            uint32_t mask = (1u << i);
            for (int j = i+1; j < usableM; ++j) {
                bool conflict = false;
                for (int k = 0; k < usableM; ++k) {
                    if (!(mask & (1u<<k))) continue;
                    if (controller.conflictMatrix[k][j]) { conflict = true; break; }
                }
                if (!conflict) mask |= (1u << j);
            }
            controller.validPhases.push_back(mask);
        }
        // greedy backward combination
        for (int i = usableM-1; i >= 0; --i) {
            uint32_t mask = (1u << i);
            for (int j = i-1; j >= 0; --j) {
                bool conflict = false;
                for (int k = 0; k < usableM; ++k) {
                    if (!(mask & (1u<<k))) continue;
                    if (controller.conflictMatrix[k][j]) { conflict = true; break; }
                }
                if (!conflict) mask |= (1u << j);
            }
            controller.validPhases.push_back(mask);
        }
        // Note: duplicates possible; keep them — harmless for decision logic
    }
    if (!controller.validPhases.empty()) controller.currentPhase = controller.validPhases[0];

    // Initialize heatMap grid based on network extents
    // fixed resolution
    const int mapW = 100;
    const int mapH = 100;
    heatMap.width = mapW;
    heatMap.height = mapH;
    // compute bounding box of nodes
    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    for (const auto &n : network.nodes) {
        minX = std::min(minX, n.position.x);
        minY = std::min(minY, n.position.y);
        maxX = std::max(maxX, n.position.x);
        maxY = std::max(maxY, n.position.y);
    }
    if (minX == std::numeric_limits<float>::infinity()) { minX = 0.0f; minY = 0.0f; maxX = 100.0f; maxY = 100.0f; }
    float spanX = std::max(1.0f, maxX - minX);
    float spanY = std::max(1.0f, maxY - minY);
    float worldMax = std::max(spanX, spanY);
    // choose cellSize so grid covers bounding box with some margin
    heatMap.cellSize = worldMax / static_cast<float>(std::max(mapW, mapH));
    heatMap.originX = minX;
    heatMap.originY = minY;
    heatMap.decayRate = 0.95f;
    heatMap.diffusionRate = 0.2f;
    heatMap.cells.assign(mapW * mapH, 0.0f);
    heatMap.buffer.assign(mapW * mapH, 0.0f);
}

void Simulation::update(float frameDt) {
    // advance simulation by frame delta time scaled by timeScale
    float effectiveDt = frameDt * timeScale;
    time += effectiveDt;
    if (!manualLightControl) controller.update(*this, effectiveDt);
    updateCars(effectiveDt);
    // ensure cars don't overlap after integration
    resolveOverlaps();
    // update congestion heat per-edge (after car positions updated, before transitions)
    updateHeat();
    // update grid-based visual heatmap (visualization only)
    updateHeatMap();
    handleEdgeTransitions();
    // after transitions, also resolve overlaps on edges
    resolveOverlaps();
    // normalize car lists per edge so ordering is consistent
    normalizeEdgeCarLists();
    // spawn new cars if desired
    spawnCars();
}

void Simulation::reset() {
    network.nodes.clear();
    network.edges.clear();
    cars.clear();
    controller.lights.clear();
    metrics.reset();
    time = 0.0f;
}

const RoadNetwork& Simulation::getNetwork() const { return network; }
const std::vector<Car>& Simulation::getCars() const { return cars; }
const TrafficController& Simulation::getController() const { return controller; }
const Metrics& Simulation::getMetrics() const { return metrics; }

// --- Scoring helpers ---
float Simulation::scoreMovementDensity(int movementId) const {
    if (movementId < 0 || movementId >= static_cast<int>(controller.movements.size())) return 0.0f;
    const auto &mv = controller.movements[movementId];
    if (!mv.incoming || !mv.outgoing) return 0.0f;
    // downstream capacity blocks scoring
    if (mv.outgoing->occupancy >= mv.outgoing->capacity) return 0.0f;
    // density = number of cars on incoming edge
    return static_cast<float>(mv.incoming->carsOnEdge.size());
}

float Simulation::scoreMovementQueue(int movementId, float threshold) const {
    if (movementId < 0 || movementId >= static_cast<int>(controller.movements.size())) return 0.0f;
    const auto &mv = controller.movements[movementId];
    if (!mv.incoming || !mv.outgoing) return 0.0f;
    if (mv.outgoing->occupancy >= mv.outgoing->capacity) return 0.0f;
    int count = 0;
    for (int idx : mv.incoming->carsOnEdge) {
        if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
        const Car &c = cars[idx];
        float distToEnd = mv.incoming->length - c.s;
        if (distToEnd < threshold || c.velocity < 0.5f) ++count;
    }
    return static_cast<float>(count);
}

float Simulation::scoreMovementPredict(int movementId, float horizon) const {
    if (movementId < 0 || movementId >= static_cast<int>(controller.movements.size())) return 0.0f;
    const auto &mv = controller.movements[movementId];
    if (!mv.incoming || !mv.outgoing) return 0.0f;
    if (mv.outgoing->occupancy >= mv.outgoing->capacity) return 0.0f;
    int count = 0;
    for (int idx : mv.incoming->carsOnEdge) {
        if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
        const Car &c = cars[idx];
        float speed = std::max(0.1f, c.velocity);
        float arrival = (mv.incoming->length - c.s) / speed;
        if (arrival < horizon) ++count;
    }
    return static_cast<float>(count);
}

float Simulation::scoreMovementHeat(int movementId) const {
    if (movementId < 0 || movementId >= static_cast<int>(controller.movements.size())) return 0.0f;
    const auto &mv = controller.movements[movementId];
    if (!mv.incoming || !mv.outgoing) return 0.0f;
    if (mv.outgoing->occupancy >= mv.outgoing->capacity) return 0.0f;
    // approximate by sampling heatmap near incoming edge end
    const auto &hm = heatMap;
    if (hm.width <= 0 || hm.height <= 0) return 0.0f;
    // world position near intersection (incoming end)
    Vec2 p = mv.incoming->end->position;
    float gx = (p.x - hm.originX) / hm.cellSize;
    float gy = (p.y - hm.originY) / hm.cellSize;
    int cx = static_cast<int>(std::floor(gx));
    int cy = static_cast<int>(std::floor(gy));
    float sum = 0.0f;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        int x = cx + dx; int y = cy + dy;
        if (x < 0 || x >= hm.width || y < 0 || y >= hm.height) continue;
        sum += hm.cells[y * hm.width + x];
    }
    return sum;
}

// Private helpers
void Simulation::updateCars(float dt) {
    // Update cars in front-to-back order per edge and lane so trailing cars see updated
    // positions of cars ahead during this timestep. This avoids two cars stopping at the
    // same stop position due to update ordering.
    std::vector<char> updated(cars.size(), 0);
    for (auto &e : network.edges) {
        // collect per-lane lists
        std::vector<int> lane0, lane1;
        for (int idx : e.carsOnEdge) {
            if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
            Car &c = cars[idx];
            if (c.currentEdge != &e) continue;
            if (c.lane == 0) lane0.push_back(idx); else lane1.push_back(idx);
        }
        auto processLane = [&](std::vector<int> &laneVec){
            if (laneVec.empty()) return;
            std::sort(laneVec.begin(), laneVec.end(), [&](int a, int b){ return cars[a].s > cars[b].s; });
            for (int idx : laneVec) {
                if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
                updateCar(cars[idx], dt);
                updated[idx] = 1;
            }
        };
        processLane(lane0);
        processLane(lane1);
    }
    // update any cars not part of edge lists (fallback)
    for (size_t i = 0; i < cars.size(); ++i) {
        if (!updated[i]) updateCar(cars[i], dt);
    }
    // collect metrics
    for (const auto &c : cars) metrics.cumulativeSpeed += c.velocity;
}

void Simulation::updateCar(Car& car, float dt) {
    // Simple behavior:
    // - if there's a red light controlling this edge and the car is within stopping
    //   distance, the car should stop before the end of the edge.
    // - if there's a car ahead, maintain a small gap and do not collide.
    // - otherwise accelerate towards desiredSpeed.

    if (!car.currentEdge) return;

    // reduce lane change cooldown
    if (car.laneChangeCooldown > 0.0f) car.laneChangeCooldown = std::max(0.0f, car.laneChangeCooldown - dt);

    // find car ahead and gap (in same lane)
    Car* ahead = getCarAhead(car);
    float gap = std::numeric_limits<float>::infinity();
    if (ahead) gap = computeGap(car, *ahead);

    // lane changing: if too close to car ahead, try to change lane if safe
    const float desiredGap = 6.0f;
    if (ahead && gap < desiredGap && car.laneChangeCooldown <= 0.0f) {
        int otherLane = 1 - car.lane;
        bool safe = true;
        // check other lane for nearby cars on same edge
        for (int idx : car.currentEdge->carsOnEdge) {
            if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
            Car &oc = cars[idx];
            if (&oc == &car) continue;
            if (oc.lane != otherLane) continue;
            // if another car is within a safety window around our position, not safe
            if (std::fabs(oc.s - car.s) < desiredGap) { safe = false; break; }
        }
        if (safe) {
            car.lane = otherLane;
            car.laneChangeCooldown = 1.0f; // small cooldown
        }
    }

    // distance to end of edge
    float distToEnd = car.currentEdge ? (car.currentEdge->length - car.s) : std::numeric_limits<float>::infinity();

    // check for red light on this edge
    bool red = approachingRedLight(car);

    // stopping distance heuristic
    float stoppingDist = car.velocity * 1.5f + 1.0f;

    float targetSpeed = car.desiredSpeed;
    if (red && distToEnd <= stoppingDist) {
        targetSpeed = 0.0f;
    }
    // respect car ahead
    if (ahead) {
        // if too close, match speed of car ahead
        if (gap < 1.0f + car.length) targetSpeed = std::min(targetSpeed, ahead->velocity);
    }

    // Ray-based detection: compute a max allowed s before collision (considers cars on this
    // edge and nearby outgoing edges). Use it to cap targetSpeed so we don't move into the
    // occupied space during this timestep.
    float maxAllowedS = std::numeric_limits<float>::infinity();
    Car* rayAhead = rayCarAhead(car, maxAllowedS, 30.0f);
    if (rayAhead && maxAllowedS < std::numeric_limits<float>::infinity()) {
        // compute allowed speed to not exceed maxAllowedS in this dt
        float allowedSpeed = (maxAllowedS - car.s) / dt;
        if (allowedSpeed < targetSpeed) {
            // if allowedSpeed is very small or negative, we'll stop
            if (allowedSpeed <= 0.01f) {
                targetSpeed = 0.0f;
            } else {
                targetSpeed = std::max(0.0f, allowedSpeed);
            }
        }
    }

    // Use an IDM-like following model when there is a car ahead to smoothly
    // adapt speed based on gap and relative velocity. Fall back to simple
    // acceleration when no car ahead.
    if (ahead) {
        // IDM parameters
        const float a0 = 1.5f;   // maximum acceleration (m/s^2)
        const float b = 2.0f;    // comfortable deceleration (m/s^2)
        const float T = 1.0f;    // desired time headway (s)
        const float s0 = 2.0f;   // minimum gap (m)
        const float delta = 4.0f; // acceleration exponent

        float s = ahead->s - car.s - ahead->length; // gap from our front to lead's rear
        s = std::max(0.1f, s);
        float deltaV = car.velocity - ahead->velocity; // positive when closing
        float sStar = s0 + car.velocity * T + (car.velocity * deltaV) / (2.0f * std::sqrt(a0 * b));
        sStar = std::max(s0, sStar);
        float freeTerm = 1.0f - std::pow((car.velocity / std::max(0.1f, car.desiredSpeed)), delta);
        float interaction = (sStar * sStar) / (s * s);
        float acc_idm = a0 * (freeTerm - interaction);
        // clamp acceleration/deceleration
        const float maxDecel = 15.0f;
        if (acc_idm < -maxDecel) acc_idm = -maxDecel;
        // apply acceleration
        car.velocity += acc_idm * dt;
        // prevent overshoot above desired speed
        if (car.velocity > car.desiredSpeed) car.velocity = car.desiredSpeed;
        if (car.velocity < 0.0f) car.velocity = 0.0f;
        // also respect targetSpeed (e.g., red light or other caps)
        if (car.velocity > targetSpeed) car.velocity = targetSpeed;
    } else {
        // simple acceleration towards targetSpeed
        const float accelUp = 5.0f;   // m/s^2
        const float accelDown = 10.0f; // braking
        if (car.velocity < targetSpeed) {
            car.velocity += accelUp * dt;
        } else if (car.velocity > targetSpeed) {
            car.velocity -= accelDown * dt;
        }
    }

    // clamp
    if (car.velocity > car.desiredSpeed) car.velocity = car.desiredSpeed;
    if (car.velocity < 0.0f) car.velocity = 0.0f;

    // compute potential new position
    float move = car.velocity * dt;
    float newS = car.s + move;

    // enforce spacing behind car ahead immediately to avoid overlaps during movement
    if (ahead) {
        float safety = 1.5f;
        float desiredGap = ahead->length + car.length + safety;
        float maxS = ahead->s - desiredGap;
        if (newS > maxS) {
            // clamp to max allowed position
            newS = std::max(car.s, maxS);
            // if we cannot move forward, stop
            if (newS <= car.s + 1e-4f) {
                car.velocity = 0.0f;
                car.waitingAtLight = true;
                metrics.totalWaitTime += dt;
                // commit (no movement)
                car.s = newS;
                return;
            }
        }
    }

            // Additional blocking: if any car (any lane) ahead is stopped/waiting near the end
            // of the edge (e.g., because it's turning and cannot enter downstream), treat it as
            // a blocker so trailing cars do not pass around it.
            {
                float nearestBlockingS = std::numeric_limits<float>::infinity();
                for (int idx : car.currentEdge->carsOnEdge) {
                    if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
                    const Car &oc = cars[idx];
                    if (&oc == &car) continue;
                    if (oc.s <= car.s) continue; // only consider ahead
                    // consider as blocking if it is essentially stopped/waiting at light
                    if (oc.waitingAtLight || oc.velocity < 0.05f) {
                        if (oc.s < nearestBlockingS) nearestBlockingS = oc.s;
                    }
                }
                if (nearestBlockingS < std::numeric_limits<float>::infinity()) {
                    float safety = 1.5f;
                    float desiredGap = 4.0f + car.length + safety; // conservative front length
                    float maxS = nearestBlockingS - desiredGap;
                    if (newS > maxS) {
                        newS = std::max(car.s, maxS);
                        if (newS <= car.s + 1e-4f) {
                            car.velocity = 0.0f;
                            car.waitingAtLight = true;
                            metrics.totalWaitTime += dt;
                            car.s = newS;
                            return;
                        }
                    }
                }
            }

    // define a stop position before the end of the edge where cars must stop for red
    float stopBuffer = std::max(1.0f, car.length + 0.5f);
    float stopPos = car.currentEdge ? (car.currentEdge->length - stopBuffer) : std::numeric_limits<float>::infinity();

    if (red) {
        // use dynamic allowed stop position which considers both the static stop line
        // and any car detected ahead via rayCarAhead (which may look into downstream edges)
        float dynamicStop = stopPos;
        if (maxAllowedS < dynamicStop) dynamicStop = maxAllowedS;

        // if already at or beyond dynamic stop, remain stopped
        if (car.s >= dynamicStop) {
            // ensure we are not overlapping a car ahead: if so, shift back
            float minPos = car.s;
            for (int idx : car.currentEdge->carsOnEdge) {
                if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
                Car &oc = cars[idx];
                if (&oc == &car) continue;
                if (oc.lane != car.lane) continue;
                if (oc.s <= car.s) continue; // only consider ahead
                float desiredGap = car.length + 0.5f;
                if (oc.s - desiredGap < minPos) minPos = oc.s - desiredGap;
            }
            car.s = std::max(0.0f, minPos);
            car.velocity = 0.0f;
            car.waitingAtLight = true;
            metrics.totalWaitTime += dt;
            return;
        }

        // if moving would cross dynamicStop, clamp to it and stop, but avoid overlapping cars ahead
        if (newS >= dynamicStop) {
            float targetPos = dynamicStop;
            for (int idx : car.currentEdge->carsOnEdge) {
                if (idx < 0 || idx >= static_cast<int>(cars.size())) continue;
                Car &oc = cars[idx];
                if (&oc == &car) continue;
                if (oc.lane != car.lane) continue;
                if (oc.s <= car.s) continue; // consider only ahead
                float desiredGap = car.length + 0.5f;
                if (oc.s - desiredGap < targetPos) targetPos = oc.s - desiredGap;
            }
            targetPos = std::max(car.s, targetPos);
            car.s = targetPos;
            car.velocity = 0.0f;
            car.waitingAtLight = true;
            metrics.totalWaitTime += dt;
            return;
        }
    }

    // otherwise commit movement
    car.s = newS;

    // waiting detection (not at light)
    car.waitingAtLight = false;
}

void Simulation::handleEdgeTransitions()
{
    // iterate by index so we can maintain carsOnEdge lists
    for (int i = 0; i < static_cast<int>(cars.size()); ++i) {
        Car &c = cars[i];
        if (!c.currentEdge) continue;
        if (c.s < c.currentEdge->length) continue;
        Edge* oldEdge = c.currentEdge;

        // remove from old edge list
        auto &vec = oldEdge->carsOnEdge;
        vec.erase(std::remove(vec.begin(), vec.end(), i), vec.end());
        oldEdge->occupancy = static_cast<int>(vec.size());

        // collect candidate outgoing edges from the intersection (exclude u-turns)
        std::vector<Edge*> candidates;
        for (auto &e : network.edges) {
            if (e.start == oldEdge->end && e.end != oldEdge->start) {
                candidates.push_back(&e);
            }
        }
        // if no non-u-turn candidates, allow u-turn (fallback)
        if (candidates.empty()) {
            for (auto &e : network.edges) {
                if (e.start == oldEdge->end) candidates.push_back(&e);
            }
        }

        Edge* chosen = nullptr;
        if (!candidates.empty()) {
            // compute incoming vector
            float inx = oldEdge->end->position.x - oldEdge->start->position.x;
            float iny = oldEdge->end->position.y - oldEdge->start->position.y;
            float inAng = std::atan2(iny, inx);
            const float PI = std::acos(-1.0f);

            // target angle depending on desiredTurn
            float target = 0.0f;
            switch (c.desiredTurn) {
            case CarTurn::Straight: target = 0.0f; break;
            case CarTurn::Left: target = PI * 0.5f; break;
            case CarTurn::Right: target = -PI * 0.5f; break;
            }

            float bestScore = std::numeric_limits<float>::infinity();
            for (Edge* e : candidates) {
                float outx = e->end->position.x - e->start->position.x;
                float outy = e->end->position.y - e->start->position.y;
                float outAng = std::atan2(outy, outx);
                float angDiff = outAng - inAng;
                // normalize to [-PI,PI]
                while (angDiff > PI) angDiff -= 2.0f * PI;
                while (angDiff <= -PI) angDiff += 2.0f * PI;
                float score = std::fabs(angDiff - target);
                if (score < bestScore) { bestScore = score; chosen = e; }
            }
        }

        if (!chosen) {
            // fallback to any edge starting at the node
            for (auto &e : network.edges) if (e.start == oldEdge->end) { chosen = &e; break; }
        }

        Edge* nextEdge = chosen ? chosen : oldEdge;

        // carry over any extra distance beyond the edge length
        float carried = 0.0f;
        if (c.s > oldEdge->length) carried = c.s - oldEdge->length;

        // Before committing transition, check if target edge has space at carried position
        bool canEnter = true;
        if (nextEdge != oldEdge) {
            // desired position on next edge
            float desiredS = carried;
            // compute conservative desired gap to any car already on next edge
            float safety = 1.5f;
            if (!nextEdge->carsOnEdge.empty()) {
                // find the nearest front car (minimum s)
                float nearestS = std::numeric_limits<float>::infinity();
                for (int idx2 : nextEdge->carsOnEdge) {
                    if (idx2 < 0 || idx2 >= static_cast<int>(cars.size())) continue;
                    const Car &oc = cars[idx2];
                    if (oc.currentEdge != nextEdge) continue;
                    if (oc.s < nearestS) nearestS = oc.s;
                }
                if (nearestS < std::numeric_limits<float>::infinity()) {
                    // find the actual car object on nextEdge with nearestS to get its length
                    const Car* frontCar = nullptr;
                    for (int idx2 : nextEdge->carsOnEdge) {
                        if (idx2 < 0 || idx2 >= static_cast<int>(cars.size())) continue;
                        const Car &oc = cars[idx2];
                        if (oc.currentEdge != nextEdge) continue;
                        if (std::fabs(oc.s - nearestS) < 1e-3f) { frontCar = &oc; break; }
                    }
                    float frontLen = frontCar ? frontCar->length : 4.0f;
                    float desiredGap = frontLen + c.length + safety;
                    if (desiredS + desiredGap >= nearestS) {
                        canEnter = false;
                    }
                }
            }
        }

        if (!canEnter && nextEdge != oldEdge) {
            // cannot transition now: stop before end of old edge and re-insert into old edge list
            float stopBuffer = std::max(1.0f, c.length + 0.5f);
            float stopPos = oldEdge->length - stopBuffer;
            c.s = std::min(stopPos, oldEdge->length);
            c.velocity = 0.0f;
            c.currentEdge = oldEdge;
            oldEdge->carsOnEdge.push_back(i);
            oldEdge->occupancy = static_cast<int>(oldEdge->carsOnEdge.size());
            c.waitingAtLight = true;
            continue;
        }

        // commit transition
        if (carried > 0.0f) c.s = carried; else c.s = 0.0f;
        c.currentEdge = nextEdge;
        nextEdge->carsOnEdge.push_back(i);
        nextEdge->occupancy = static_cast<int>(nextEdge->carsOnEdge.size());

        // after crossing an intersection, randomly pick a new desiredTurn using weighted probabilities
        static std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
        float ps = probStraight; float pl = probLeft; float pr = probRight;
        float sum = ps + pl + pr;
        if (sum <= 0.0f) { ps = 0.6f; pl = 0.2f; pr = 0.2f; sum = 1.0f; }
        ps /= sum; pl /= sum; pr /= sum;
        std::uniform_real_distribution<float> ud(0.0f, 1.0f);
        float rv = ud(rng);
        if (rv < ps) c.desiredTurn = CarTurn::Straight;
        else if (rv < ps + pl) c.desiredTurn = CarTurn::Left;
        else c.desiredTurn = CarTurn::Right;

        metrics.carsPassed++;
    }
}

void Simulation::spawnCars() {
    // Simple deterministic spawning: ensure at least one car per edge up to a limit.
    // Increased default maxCars so runtime spawning can populate a denser network.
    const int maxCars = 200;
    if (cars.size() >= static_cast<size_t>(maxCars)) return;

    for (size_t ei = 0; ei < network.edges.size(); ++ei) {
        if (cars.size() >= static_cast<size_t>(maxCars)) break;
        Edge &e = network.edges[ei];
        // if there is already a car near the start, skip
        bool occupied = false;
        for (int idx : e.carsOnEdge) {
            if (idx >= 0 && idx < static_cast<int>(cars.size())) {
                if (cars[idx].s < 1.0f) { occupied = true; break; }
            }
        }
        if (occupied) continue;

        Car c;
        c.id = static_cast<int>(cars.size());
        c.currentEdge = &e;
        c.s = 0.0f;
        c.desiredSpeed = e.speedLimit;
        // give cars an initial forward velocity so they begin moving
        c.velocity = c.desiredSpeed * 0.5f;
        c.acceleration = 0.0f;
        c.length = 4.0f;
        c.waitingAtLight = false;
        c.desiredTurn = CarTurn::Straight;
        c.lane = 1; // start in right lane
        c.laneChangeCooldown = 0.0f;

        cars.push_back(c);
        e.carsOnEdge.push_back(static_cast<int>(cars.size() - 1));
        e.occupancy = static_cast<int>(e.carsOnEdge.size());
    }
}

Car* Simulation::getCarAhead(const Car& car) {
    // Find index of the car in our cars vector
    const Car* carPtr = &car;
    int idx = -1;
    for (size_t i = 0; i < cars.size(); ++i) {
        if (&cars[i] == carPtr) { idx = static_cast<int>(i); break; }
    }
    if (idx < 0) return nullptr;
    Car& subject = cars[idx];
    Edge* edge = subject.currentEdge;
    if (!edge) return nullptr;

    Car* best = nullptr;
    float bestDelta = std::numeric_limits<float>::infinity();
    for (int carIndex : edge->carsOnEdge) {
        if (carIndex < 0 || carIndex >= static_cast<int>(cars.size())) continue;
        if (carIndex == idx) continue;
        Car& other = cars[carIndex];
        if (other.lane != subject.lane) continue; // only consider same lane
        if (other.s <= subject.s) continue; // ahead must have larger s
        float delta = other.s - subject.s;
        if (delta < bestDelta) {
            bestDelta = delta;
            best = &other;
        }
    }
    return best;
}

bool Simulation::approachingRedLight(const Car& car) const
{
    if (!car.currentEdge) return false;
    for (const auto &light : controller.lights) {
        if (light.controlledEdge == car.currentEdge) {
            // if the light allows the car's desired turn, it's not red for that car
            switch (car.desiredTurn) {
                case CarTurn::Straight: return !light.allowStraight;
                case CarTurn::Left: return !light.allowLeft;
                case CarTurn::Right: return !light.allowRight;
            }
            return true;
        }
    }
    return false;
}

float Simulation::computeGap(const Car& a, const Car& b) const {
    // gap along the same edge from a to b (b ahead of a)
    if (!a.currentEdge || a.currentEdge != b.currentEdge) return std::numeric_limits<float>::infinity();
    return std::max(0.0f, b.s - a.s - a.length);
}

// Concrete strategy implementations (inherit from top-level TrafficStrategy)
// Fixed cycle: rotate through valid phases in order
struct FixedCycleStrategy : public TrafficStrategy {
    size_t index = 0;
    uint32_t choosePhase(Simulation& /*sim*/, TrafficController& controller) override {
        if (controller.validPhases.empty()) return 0;
        // find current index
        size_t M = controller.validPhases.size();
        // advance to next phase from currentPhase
        size_t cur = 0;
        for (size_t i = 0; i < M; ++i) if (controller.validPhases[i] == controller.currentPhase) { cur = i; break; }
        index = (cur + 1) % M;
        return static_cast<uint32_t>(controller.validPhases[index]);
    }
};

// scoring-based strategies: utility function
static float scorePhaseBy(const Simulation& sim, const TrafficController& controller, const uint32_t &phase,
                          std::function<float(int)> scorer)
{
    float sum = 0.0f;
    int M = static_cast<int>(controller.movements.size());
    for (int mi = 0; mi < M; ++mi) {
        if (phase & (1u << mi)) sum += scorer(mi);
    }
    return sum;
}

struct DensityStrategy : public TrafficStrategy {
    uint32_t choosePhase(Simulation& sim, TrafficController& controller) override {
        uint32_t best = controller.currentPhase;
        float bestScore = -1e9f;
        for (auto phase : controller.validPhases) {
            float s = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementDensity(m); });
            if (s > bestScore) { bestScore = s; best = static_cast<uint32_t>(phase); }
        }
        return best;
    }
};

struct QueueStrategy : public TrafficStrategy {
    uint32_t choosePhase(Simulation& sim, TrafficController& controller) override {
        uint32_t best = controller.currentPhase;
        float bestScore = -1e9f;
        for (auto phase : controller.validPhases) {
            float s = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementQueue(m, 15.0f); });
            if (s > bestScore) { bestScore = s; best = static_cast<uint32_t>(phase); }
        }
        return best;
    }
};

struct HeatMapStrategy : public TrafficStrategy {
    uint32_t choosePhase(Simulation& sim, TrafficController& controller) override {
        uint32_t best = controller.currentPhase;
        float bestScore = -1e9f;
        for (auto phase : controller.validPhases) {
            float s = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementHeat(m); });
            if (s > bestScore) { bestScore = s; best = static_cast<uint32_t>(phase); }
        }
        return best;
    }
};

struct PredictiveStrategy : public TrafficStrategy {
    uint32_t choosePhase(Simulation& sim, TrafficController& controller) override {
        uint32_t best = controller.currentPhase;
        float bestScore = -1e9f;
        for (auto phase : controller.validPhases) {
            float s = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementPredict(m, 5.0f); });
            if (s > bestScore) { bestScore = s; best = static_cast<uint32_t>(phase); }
        }
        return best;
    }
};

struct HybridStrategy : public TrafficStrategy {
    float wDensity = 1.0f, wQueue = 1.0f, wPredict = 1.0f, wHeat = 1.0f;
    uint32_t choosePhase(Simulation& sim, TrafficController& controller) override {
        uint32_t best = controller.currentPhase;
        float bestScore = -1e9f;
        for (auto phase : controller.validPhases) {
            float sd = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementDensity(m); });
            float sq = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementQueue(m, 15.0f); });
            float sp = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementPredict(m, 5.0f); });
            float sh = scorePhaseBy(sim, controller, static_cast<uint32_t>(phase), [&](int m){ return sim.scoreMovementHeat(m); });
            float total = wDensity*sd + wQueue*sq + wPredict*sp + wHeat*sh;
            if (total > bestScore) { bestScore = total; best = static_cast<uint32_t>(phase); }
        }
        return best;
    }
};

// Set controller strategy by index (0=Fixed,1=Density,2=Queue,3=Heat,4=Predictive,5=Hybrid)
void Simulation::setControllerStrategyByIndex(int idx)
{
    if (idx < 0) return;
    switch (idx) {
    case 0:
        controller.setStrategy(std::make_unique<FixedCycleStrategy>());
        break;
    case 1:
        controller.setStrategy(std::make_unique<DensityStrategy>());
        break;
    case 2:
        controller.setStrategy(std::make_unique<QueueStrategy>());
        break;
    case 3:
        controller.setStrategy(std::make_unique<HeatMapStrategy>());
        break;
    case 4:
        controller.setStrategy(std::make_unique<PredictiveStrategy>());
        break;
    case 5:
        controller.setStrategy(std::make_unique<HybridStrategy>());
        break;
    default:
        controller.setStrategy(std::make_unique<DensityStrategy>());
        break;
    }
}

// Controller update uses strategy to pick safe phase masks
void TrafficController::update(Simulation &sim, float dt)
{
    phaseTimer += dt;
    if (!strategy) return;

    // helper: check if an edge has any allowed movement in a given phase mask
    auto edgeAllowedInPhase = [&](Edge* edge, PhaseMask phase)->bool {
        if (!edge) return false;
        for (size_t mi = 0; mi < movements.size(); ++mi) {
            if (!(phase & (1u << static_cast<unsigned>(mi)))) continue;
            const auto &mv = movements[mi];
            if (mv.incoming == edge) return true;
        }
        return false;
    };

    // update per-light timers (how long they've been allowed in the currentPhase)
    for (auto &light : lights) {
        bool currAllowed = edgeAllowedInPhase(light.controlledEdge, currentPhase);
        if (currAllowed) light.timer += dt; else light.timer = 0.0f;
    }

    // decide whether to evaluate a new phase (controller-level gating)
    bool force = (maxGreenTime > 0.0f && phaseTimer >= maxGreenTime);
    bool canDecide = (phaseTimer >= minGreenTime) || force;
    if (canDecide) {
        auto desired = strategy->choosePhase(sim, *this);
        if (desired != currentPhase) {
            // Check per-light minimum green: do not turn off any light that hasn't satisfied its minGreen
            bool blockedByLightMin = false;
            if (!force) {
                for (auto &light : lights) {
                    bool currently = edgeAllowedInPhase(light.controlledEdge, currentPhase);
                    bool future = edgeAllowedInPhase(light.controlledEdge, desired);
                    if (currently && !future) {
                        if (light.timer < light.minGreen) { blockedByLightMin = true; break; }
                    }
                }
            }
            if (!blockedByLightMin || force) {
                currentPhase = desired;
                phaseTimer = 0.0f;
                // reset timers for lights that just became active will be handled in next update iteration
            }
        }
    }

    // Update legacy TrafficLight allow flags & state to reflect currentPhase
    // For each light, find movements whose incoming edge matches the controlled edge
    const float PI = std::acos(-1.0f);
    for (auto &light : lights) {
        // reset
        light.allowStraight = false;
        light.allowLeft = false;
        light.allowRight = false;
        // if the light has been inactive for a long time ensure timer is clamped
        if (light.timer < 0.0f) light.timer = 0.0f;
        if (!light.controlledEdge) {
            light.state = LightState::Red;
            continue;
        }
        // compute incoming angle
        Vec2 inS = light.controlledEdge->start->position;
        Vec2 inE = light.controlledEdge->end->position;
        float inAng = std::atan2(inE.y - inS.y, inE.x - inS.x);
        // scan movements
        for (size_t mi = 0; mi < movements.size(); ++mi) {
            const auto &mv = movements[mi];
            if (!mv.incoming || !mv.outgoing) continue;
            if (mv.incoming != light.controlledEdge) continue;
            // if this movement is currently allowed in the phase
            if ((currentPhase & (1u << static_cast<unsigned>(mi))) == 0) continue;
            // compute outgoing angle
            Vec2 outS = mv.outgoing->start->position;
            Vec2 outE = mv.outgoing->end->position;
            float outAng = std::atan2(outE.y - outS.y, outE.x - outS.x);
            float angDiff = outAng - inAng;
            while (angDiff > PI) angDiff -= 2.0f * PI;
            while (angDiff <= -PI) angDiff += 2.0f * PI;
            float absd = std::fabs(angDiff);
            // classify: straight if near 0, left if positive, right if negative
            if (absd < (PI * 0.35f)) {
                light.allowStraight = true;
            } else if (angDiff > 0.0f) {
                light.allowLeft = true;
            } else {
                light.allowRight = true;
            }
        }
        // set state for rendering
        if (!light.allowStraight && !light.allowLeft && !light.allowRight) light.state = LightState::Red;
        else if (light.allowStraight) light.state = LightState::Straight;
        else if (light.allowLeft) light.state = LightState::Left;
        else light.state = LightState::Right;
    }
}
