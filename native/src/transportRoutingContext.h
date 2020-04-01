#ifndef _OSMAND_TRANSPORT_ROUTING_CONTEXT_H
#define _OSMAND_TRANSPORT_ROUTING_CONTEXT_H
#include "CommonCollections.h"
#include "commonOsmAndCore.h"
#include "binaryRead.h"
#include "transportRoutingConfiguration.h"
#include "transportRoutingObjects.h"
#include "routeCalculationProgress.h"
#include "transportRoutePlanner.h"
#include "transportRouteSegment.h"
#include "ElapsedTimer.h"

struct TransportRoutingContext {
    
    SHARED_PTR<RouteCalculationProgress> calculationProgress;
    UNORDERED(map)<int64_t, SHARED_PTR<TransportRouteSegment>> visitedSegments;
    SHARED_PTR<TransportRoutingConfiguration> cfg;

    UNORDERED(map)<int64_t, std::vector<SHARED_PTR<TransportRouteSegment>>> quadTree;

    UNORDERED(map)<BinaryMapFile*, UNORDERED(map)<int64_t, SHARED_PTR<TransportRoute>>> routeMap;

    int32_t startX;
    int32_t startY;
    int32_t targetX;
    int32_t targetY;

    double startLat;
    double startLon;
    double endLat;
    double endLon;

    int64_t startCalcTime;
    int32_t visitedRoutesCount;
    int32_t visitedStops;
    int32_t wrongLoadedWays;
    int32_t loadedWays;
    OsmAnd::ElapsedTimer loadTime;
    OsmAnd::ElapsedTimer readTime;

    int32_t walkRadiusIn31;
    int32_t walkChangeRadiusIn31;

    TransportRoutingContext(SHARED_PTR<TransportRoutingConfiguration> cfg_)
    {
        cfg = cfg_;
        walkRadiusIn31 = (int) (cfg->walkRadius / getTileDistanceWidth(31));
        walkChangeRadiusIn31 = (int) (cfg->walkChangeRadius / getTileDistanceWidth(31));
        for (BinaryMapFile* i : getOpenMapFiles())  {
            routeMap.insert({i, UNORDERED(map)<int64_t, SHARED_PTR<TransportRoute>>()});
        }
    }
    
    inline static double getTileDistanceWidth(float zoom)
    {
        double lat1 = 30;
        double lon1 = getLongitudeFromTile(zoom, 0);
        double lat2 = 30;
        double lon2 = getLongitudeFromTile(zoom, 1);
        return getDistance(lon1, lat1, lon2, lat2);
    }
    
    //todo need to be checked, unclear if its same result as in java
    void calcLatLons() {
        startLat = getLatitudeFromTile(TRANSPORT_STOP_ZOOM, startY);
        startLon = getLongitudeFromTile(TRANSPORT_STOP_ZOOM, startX);
        endLat = getLatitudeFromTile(TRANSPORT_STOP_ZOOM, targetY);
        endLon = getLongitudeFromTile(TRANSPORT_STOP_ZOOM, targetX);
    }

    std::vector<SHARED_PTR<TransportRouteSegment>> getTransportStops(int32_t sx, int32_t sy, bool change, vector<SHARED_PTR<TransportRouteSegment>> res) {
        loadTime.Start();
        int32_t d = change ? walkChangeRadiusIn31 : walkRadiusIn31;
        int32_t lx = (sx - d) >> (31 - cfg->zoomToLoadTiles);
        int32_t rx = (sx + d) >> (31 - cfg->zoomToLoadTiles);
        int32_t ty = (sy - d) >> (31 - cfg->zoomToLoadTiles);
        int32_t by = (sy + d) >> (31 - cfg->zoomToLoadTiles);
        for(int32_t x = lx; x <= rx; x++) {
            for(int32_t y = ty; y <= by; y++) {
                int64_t tileId = (((int64_t) x) << (cfg->zoomToLoadTiles + 1)) + y;
                vector<SHARED_PTR<TransportRouteSegment>> list = quadTree.find(tileId)->second;
                if (quadTree.find(tileId) == quadTree.end()) {
                    list = loadTile(x, y);
                    quadTree.insert({tileId, list});
                }
                for (SHARED_PTR<TransportRouteSegment> it : list) {
                    SHARED_PTR<TransportStop> st = it->getStop(it->segStart);
                    if (abs(st->x31 - sx) > walkRadiusIn31 || abs(st->y31 - sy) > walkRadiusIn31) {
                        wrongLoadedWays++;
                    } else {
                        loadedWays++;
                        res.push_back(it);
                    }
                }
            }
        }
        loadTime.Pause();
        return res;
    }

    std::vector<SHARED_PTR<TransportRouteSegment>> loadTile(uint32_t x, uint32_t y) {
        //long nanoTime = System.nanoTime();
        vector<SHARED_PTR<TransportRouteSegment>> lst;
        int pz = (31 - cfg->zoomToLoadTiles);
        
        SearchQuery *q = new SearchQuery((uint32_t) (x << pz), (uint32_t) ((x + 1) << pz), (uint32_t)(y << pz), (uint32_t)((y+1) << pz));
        UNORDERED(map)<int64_t, SHARED_PTR<TransportStop>> loadedTransportStops;
        UNORDERED(map)<int64_t, SHARED_PTR<TransportRoute>> localFileRoutes;
        vector<SHARED_PTR<TransportStop>> loadedTransportStopsVals;
        
        vector<SHARED_PTR<TransportStop>> stops;
        
        auto openFiles = getOpenMapFiles();
        std::vector<BinaryMapFile*>::iterator it, end;
        for (it = openFiles.begin(), end = openFiles.end(); it != end; ++it) {
            q->transportResults.clear();
            searchTransportIndex(q, *it);
            stops = q->transportResults;
            localFileRoutes.clear();
            mergeTransportStops(*it, loadedTransportStops, stops, localFileRoutes, routeMap[*it]);
        
            for (SHARED_PTR<TransportStop> stop : stops) {
                int64_t stopId = stop->id;
                SHARED_PTR<TransportStop> multifileStop = loadedTransportStops.find(stopId)->second;
                vector<int32_t> rrs = stop->referencesToRoutes;
                if (multifileStop == stop) {
                    // clear up so it won't be used as it is multi file stop
                    stop->referencesToRoutes.clear();
                } else {
                    // add other routes
                    stop->referencesToRoutes.clear();
                }
                if (rrs.size() > 0 && !multifileStop->isDeleted()) {
                    for (int32_t& rr : rrs) {
                        SHARED_PTR<TransportRoute> route = localFileRoutes.at(rr);
                        if (route == nullptr) {
                            // TODO: add stop name
//                            OsmAnd::LogPrintf(OsmAnd::LogSeverityLevel::Error, "Something went wrong by loading route %d for stop ", rr);
                        } else if (multifileStop == stop ||
                                (!multifileStop->hasRoute(route->id) &&
                                        !multifileStop->isRouteDeleted(route->id))) {
                            // duplicates won't be added check!
                            multifileStop->addRouteId(route->id);
                            multifileStop->addRoute(route);
                        }
                    }
                }
            }
        }
        std::vector<SHARED_PTR<TransportStop>> stopsValues;
        stopsValues.reserve(loadedTransportStops.size());
 
        // Get all values
        std::transform (loadedTransportStops.begin(), loadedTransportStops.end(), back_inserter(stopsValues), [] (std::pair<int64_t, SHARED_PTR<TransportStop>> const & pair)
                        {
            return pair.second;
        });

        loadTransportSegments(stopsValues, lst);
        
        // readTime += System.nanoTime() - nanoTime;
        return lst;
    }

    std::vector<SHARED_PTR<TransportStop>> mergeTransportStops(
        //TODO change for native loading mechanic
        BinaryMapFile* file, 
        UNORDERED(map)<int64_t, SHARED_PTR<TransportStop>>& loadedTransportStops,
        vector<SHARED_PTR<TransportStop>>& stops,
        UNORDERED(map)<int64_t, SHARED_PTR<TransportRoute>>& localFileRoutes,
        UNORDERED(map)<int64_t, SHARED_PTR<TransportRoute>>& loadedRoutes) {

        vector<int32_t> routesToLoad;
        vector<int32_t> localRoutesToLoad;
        
        vector<SHARED_PTR<TransportStop>>::iterator it = stops.begin();
        while(it != stops.end()) {
            int64_t stopId = (*it)->id;
            localRoutesToLoad.clear();
            SHARED_PTR<TransportStop> multifileStop = loadedTransportStops.find(stopId)->second;
            vector<int64_t> routesIds = (*it)->routesIds;
            vector<int64_t> delRIds = (*it)->deletedRoutesIds;
            if (loadedTransportStops.find(stopId) == loadedTransportStops.end()) {
                loadedTransportStops.insert({stopId, *it});
//                loadedTransportStopsVals.push_back(*it);
                multifileStop = *it;
                if (!(*it)->isDeleted()) {
                    localRoutesToLoad.insert(localRoutesToLoad.end(), (*it)->referencesToRoutes.begin(), (*it)->referencesToRoutes.end());
                }
            } else if (multifileStop->isDeleted()) {
                it = stops.erase(it);
            } else {
                if (delRIds.size() > 0) {
                    for (vector<int64_t>::iterator it = delRIds.begin(); it != delRIds.end(); it++) {
                        multifileStop->deletedRoutesIds.push_back(*it);
                    }
                }
                if (routesIds.size() > 0) {
                    vector<int32_t> refs = (*it)->referencesToRoutes;
                    for (int32_t i = 0; i < routesIds.size(); i++) {
                        int64_t routeId = routesIds.at(i);
                        if (find(routesIds.begin(), routesIds.end(), routeId) == multifileStop->routesIds.end()
                        && multifileStop->isRouteDeleted(routeId)) {
                            localRoutesToLoad.push_back(refs[i]);
                        }
                    }
                } else {
                    if ((*it)->hasReferencesToRoutes()) {
                        localRoutesToLoad.insert(localRoutesToLoad.end(), (*it)->referencesToRoutes.begin(), (*it)->referencesToRoutes.end());
                    } else {
                        it = stops.erase(it);
                    }
                }
            }
            routesToLoad.insert(routesToLoad.end(), localRoutesToLoad.begin(), localRoutesToLoad.end());
            
        //TODO get name of file. 
            multifileStop->putReferenceToRoutes(file->inputName, localRoutesToLoad);
            it++;
        }

        if (routesToLoad.size() > 0) {
            sort(routesToLoad.begin(), routesToLoad.end());
            vector<int32_t> referencesToLoad;
            vector<int32_t>::iterator itr = routesToLoad.begin();
            int32_t p = routesToLoad.at(0) + 1;
            while (itr != routesToLoad.end()) {
                int nxt = *itr;
                if (p != nxt) {
                    if (loadedRoutes.find(nxt) != loadedRoutes.end()) {
                        localFileRoutes.insert(std::pair<int64_t, SHARED_PTR<TransportRoute>>(nxt, loadedRoutes.find(nxt)->second));
                    } else {
                        referencesToLoad.push_back(nxt);
                    }
                }
                itr++;
            }
        //todo: what to use?
            
            loadTransportRoutes(file, referencesToLoad, localFileRoutes);
            loadedRoutes.insert(localFileRoutes.begin(), localFileRoutes.end());
        }

        return stops;

    }

    void loadTransportSegments(vector<SHARED_PTR<TransportStop>>& stops, vector<SHARED_PTR<TransportRouteSegment>>& lst) {
        for(SHARED_PTR<TransportStop> s : stops) {
            if (s->isDeleted() || s->routes.size() == 0) {
                continue;
            }
            for (SHARED_PTR<TransportRoute>& route : s->routes) {
                int stopIndex = -1;
                double dist = SAME_STOP;
                for (int k = 0; k < route->forwardStops.size(); k++) {
                    SHARED_PTR<TransportStop> st = route->forwardStops.at(k);
                    if(st->id == s->id ) {
                        stopIndex = k;
                        break;
                    }
                    double d = getDistance(st->lat, st->lon, s->lat, s->lon);
                    if (d < dist) {
                        stopIndex = k;
                        dist = d;
                    }
                }
                if (stopIndex != -1) {
                    if (cfg->useSchedule) {
                        loadScheduleRouteSegment(lst, route, stopIndex);
                    } else {
                        const auto segment = make_shared<TransportRouteSegment>(route, stopIndex);
                        lst.push_back(segment);
                    }
                } else {
                    // TODO: Log
//                    OsmAnd::LogPrintf(OsmAnd::LogSeverityLevel::Error, "Routing error: missing stop '%s' in route '%s' id: %d", s->toString(), route->ref, route->id / 2));
                }
            }
		}
    }


    void loadScheduleRouteSegment(std::vector<SHARED_PTR<TransportRouteSegment>> lst, SHARED_PTR<TransportRoute>& route, int32_t stopIndex) {
        if(route->schedule != nullptr) {
            vector<int32_t> ti = route->schedule->tripIntervals;
            int32_t cnt = ti.size();
            int32_t t = 0;
            // improve by using exact data
            int32_t stopTravelTime = 0;
            vector<int32_t> avgStopIntervals = route->schedule->avgStopIntervals;
            for (int32_t i = 0; i < stopIndex; i++) {
                if (avgStopIntervals.size() > i) {
                    stopTravelTime += avgStopIntervals[i];
                }
            }
            for(int32_t i = 0; i < cnt; i++) {
                t += ti[i];
                int32_t startTime = t + stopTravelTime;
                if(startTime >= cfg->scheduleTimeOfDay && startTime <= cfg->scheduleTimeOfDay + cfg->scheduleMaxTime) {
                    const auto segment = make_shared<TransportRouteSegment>(route, stopIndex, startTime);
                    lst.push_back(segment);
                }
            }
        }
    }
};

#endif // _OSMAND_TRANSPORT_ROUTING_CONTEXT_H
