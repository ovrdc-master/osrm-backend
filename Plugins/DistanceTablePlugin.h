/*

Copyright (c) 2014, Project OSRM, Dennis Luxen, others
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef DISTANCE_TABLE_PLUGIN_H
#define DISTANCE_TABLE_PLUGIN_H

#include "BasePlugin.h"

#include "../Algorithms/object_encoder.hpp"
#include "../DataStructures/QueryEdge.h"
#include "../DataStructures/SearchEngine.h"
#include "../Descriptors/BaseDescriptor.h"
#include "../Util/json_renderer.hpp"
#include "../Util/make_unique.hpp"
#include "../Util/StringUtil.h"
#include "../Util/TimingUtil.h"

#include <osrm/json_container.hpp>

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

template <class DataFacadeT> class DistanceTablePlugin final : public BasePlugin
{
  private:
    std::unique_ptr<SearchEngine<DataFacadeT>> search_engine_ptr;

  public:
    explicit DistanceTablePlugin(DataFacadeT *facade) : descriptor_string("table"), facade(facade)
    {
        search_engine_ptr = osrm::make_unique<SearchEngine<DataFacadeT>>(facade);
    }

    virtual ~DistanceTablePlugin() {}

    const std::string GetDescriptor() const final { return descriptor_string; }

    void HandleRequest(const RouteParameters &route_parameters, http::Reply &reply) final
    {
        if (!check_all_coordinates(route_parameters.coordinates))
        {
            reply = http::Reply::StockReply(http::Reply::badRequest);
            return;
        }

        const bool checksum_OK = (route_parameters.check_sum == facade->GetCheckSum());
        unsigned max_locations =
            std::min(100u, static_cast<unsigned>(route_parameters.coordinates.size()));
        PhantomNodeArray phantom_node_vector(max_locations);
        for (const auto i : osrm::irange(1u, max_locations))
        {
            if (checksum_OK && i < route_parameters.hints.size() &&
                !route_parameters.hints[i].empty())
            {
                PhantomNode current_phantom_node;
                ObjectEncoder::DecodeFromBase64(route_parameters.hints[i], current_phantom_node);
                if (current_phantom_node.is_valid(facade->GetNumberOfNodes()))
                {
                    phantom_node_vector[i].emplace_back(std::move(current_phantom_node));
                    continue;
                }
            }
            facade->IncrementalFindPhantomNodeForCoordinate(route_parameters.coordinates[i],
                                                            phantom_node_vector[i],
                                                            route_parameters.zoom_level,
                                                            1);

            BOOST_ASSERT(phantom_node_vector[i].front().is_valid(facade->GetNumberOfNodes()));
        }

        // TIMER_START(distance_table);
        std::shared_ptr<std::vector<EdgeWeight>> result_table =
            search_engine_ptr->distance_table(phantom_node_vector);
        // TIMER_STOP(distance_table);

        if (!result_table)
        {
            reply = http::Reply::StockReply(http::Reply::badRequest);
            return;
        }
        JSON::Object json_object;
        JSON::Array json_array;
        const unsigned number_of_locations = static_cast<unsigned>(phantom_node_vector.size());
        for (unsigned row = 0; row < number_of_locations; ++row)
        {
            JSON::Array json_row;
            auto row_begin_iterator = result_table->begin() + (row * number_of_locations);
            auto row_end_iterator = result_table->begin() + ((row + 1) * number_of_locations);
            json_row.values.insert(json_row.values.end(), row_begin_iterator, row_end_iterator);
            json_array.values.push_back(json_row);
        }
        json_object.values["distance_table"] = json_array;
        JSON::render(reply.content, json_object);
    }

  private:
    std::string descriptor_string;
    DataFacadeT *facade;
};

#endif // DISTANCE_TABLE_PLUGIN_H
