#include "elaborator_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    void UnitElaborator::recordDriver(const ObjectWire& object, const std::string& driver, bool fromProcess, const ASTNode& at)
    {
        DriverSet& drivers = m_drivers[object.declaration];
        if (drivers.name.empty())
        {
            drivers.name = object.name;
            drivers.location = at.source;
            drivers.isPort = object.objectClass == ObjectWire::Class::Port;
        }

        std::vector<std::string>& list = fromProcess ? drivers.processes : drivers.others;
        if (std::find(list.begin(), list.end(), driver) == list.end())
            list.push_back(driver);
    }

    /// A process drives a wire directly (Wire::drive), which the wire ignores once it has a source, and two processes would
    /// overwrite each other instead of being resolved. So a signal a process assigns has that process as its only driver.
    void UnitElaborator::checkDrivers()
    {
        for (const auto& [declaration, drivers] : m_drivers)
        {
            if (drivers.processes.empty())
                continue;

            const size_t count = drivers.processes.size() + drivers.others.size();
            if (count > 1)
            {
                std::string others;
                for (size_t i = 1; i < drivers.processes.size(); ++i)
                    others += (others.empty() ? "" : ", ") + drivers.processes[i];
                for (const std::string& other : drivers.others)
                    others += (others.empty() ? "" : ", ") + other;

                throw elaboration_error("Signal '" + drivers.name + "' is assigned by " + drivers.processes.front() + " and also driven by "
                                        + others + "; a signal assigned in a process must have that process as its only driver "
                                        "(several drivers of such a signal are not supported yet)", drivers.location);
            }

            if (drivers.isPort)
                m_spec.processDrivenPorts.insert(drivers.name);
        }
    }

} // namespace Pulse::Parser
