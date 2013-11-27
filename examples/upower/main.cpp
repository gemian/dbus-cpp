/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include "upower.h"

#include <org/freedesktop/dbus/bus.h>
#include <org/freedesktop/dbus/object.h>
#include <org/freedesktop/dbus/property.h>
#include <org/freedesktop/dbus/service.h>

#include <org/freedesktop/dbus/asio/executor.h>
#include <org/freedesktop/dbus/interfaces/properties.h>
#include <org/freedesktop/dbus/types/struct.h>
#include <org/freedesktop/dbus/types/stl/tuple.h>
#include <org/freedesktop/dbus/types/stl/vector.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/moment.hpp>

#include <sys/types.h>
#include <signal.h>

namespace acc = boost::accumulators;
namespace dbus = org::freedesktop::dbus;

namespace
{
dbus::Bus::Ptr the_system_bus()
{
    static dbus::Bus::Ptr system_bus = std::make_shared<dbus::Bus>(dbus::WellKnownBus::system);
    return system_bus;
}
}

int main(int, char**)
{
    auto bus = the_system_bus();
bus->install_executor(org::freedesktop::dbus::asio::make_executor(bus));
    std::thread t {std::bind(&dbus::Bus::run, bus)};
    auto upower = dbus::Service::use_service(bus, dbus::traits::Service<org::freedesktop::UPower>::interface_name());
    auto upower_object = upower->object_for_path(dbus::types::ObjectPath("/org/freedesktop/UPower"));

    auto all_properties = upower_object->get_all_properties<org::freedesktop::UPower>();
    std::for_each(all_properties.begin(), all_properties.end(), [](const std::pair<const std::string, dbus::types::Variant<dbus::types::Any>>& pair)
    {
        std::cout << pair.first << std::endl;
    });

    std::cout << upower_object->get_property<org::freedesktop::UPower::Properties::DaemonVersion>()->get() << std::endl;
    std::cout << upower_object->get_property<org::freedesktop::UPower::Properties::CanSuspend>()->get() << std::endl;
    std::cout << upower_object->get_property<org::freedesktop::UPower::Properties::CanHibernate>()->get() << std::endl;
    std::cout << upower_object->get_property<org::freedesktop::UPower::Properties::OnBattery>()->get() << std::endl;
    std::cout << upower_object->get_property<org::freedesktop::UPower::Properties::LidIsClosed>()->get() << std::endl;
    std::cout << upower_object->get_property<org::freedesktop::UPower::Properties::LidIsPresent>()->get() << std::endl;

    auto device_added_signal = upower_object->get_signal<org::freedesktop::UPower::Signals::DeviceAdded>();
    device_added_signal->connect([](const org::freedesktop::UPower::Signals::DeviceAdded::ArgumentType&)
    {
        std::cout << "org::freedesktop::UPower::Signals::DeviceAdded" << std::endl;
    });

    auto device_removed_signal = upower_object->get_signal<org::freedesktop::UPower::Signals::DeviceRemoved>();
    device_removed_signal->connect([](const org::freedesktop::UPower::Signals::DeviceRemoved::ArgumentType&)
    {
        std::cout << "org::freedesktop::UPower::Signals::DeviceRemoved" << std::endl;
    });

    auto device_changed_signal = upower_object->get_signal<org::freedesktop::UPower::Signals::DeviceChanged>();
    device_changed_signal->connect([](const org::freedesktop::UPower::Signals::DeviceChanged::ArgumentType&)
    {
        std::cout << "org::freedesktop::UPower::Signals::DeviceChanged" << std::endl;
    });

    auto changed_signal = upower_object->get_signal<org::freedesktop::UPower::Signals::Changed>();
    changed_signal->connect([]()
    {
        std::cout << "org::freedesktop::UPower::Signals::Changed" << std::endl;
    });

    auto sleeping_signal = upower_object->get_signal<org::freedesktop::UPower::Signals::Sleeping>();
    sleeping_signal->connect([]()
    {
        std::cout << "org::freedesktop::UPower::Signals::Sleeping" << std::endl;
    });

    auto resuming_signal = upower_object->get_signal<org::freedesktop::UPower::Signals::Resuming>();
    resuming_signal->connect([]()
    {
        std::cout << "org::freedesktop::UPower::Signals::Resuming" << std::endl;
    });

    auto devices = upower_object->invoke_method_synchronously<org::freedesktop::UPower::EnumerateDevices, std::vector<dbus::types::ObjectPath>>();
    std::cout << "Devices count: " << devices.value().size() << std::endl;
    std::for_each(devices.value().begin(), devices.value().end(), [upower_object](const dbus::types::ObjectPath& path)
    {
        std::cout << "===================================================================================" << std::endl;
        auto device = upower_object->add_object_for_path(path);
        auto device_changed_signal = device->get_signal<org::freedesktop::UPower::Device::Signals::Changed>();
        device_changed_signal->connect([]()
        {
            std::cout << "org::freedesktop::UPower::Device::Signals::Changed" << std::endl;
        });
        try
        {
            auto stats = device->invoke_method_synchronously<
                         org::freedesktop::UPower::Device::GetStatistics,
                         std::vector<dbus::types::Struct<std::tuple<double, double>>>,
                         std::string>("charging");
            std::for_each(stats.value().begin(), stats.value().end(), [](const dbus::types::Struct<std::tuple<double, double>>& t)
            {
                std::cout << std::get<0>(t.value) << ", " << std::get<1>(t.value) << std::endl;
            });
        }
        catch (const std::runtime_error& e)
        {
            std::cout << e.what() << std::endl;
        }

        try
        {
            acc::accumulator_set<double, acc::stats<acc::tag::mean, acc::tag::moment<2> > > as;
            auto history = device->invoke_method_synchronously<
                           org::freedesktop::UPower::Device::GetHistory,
                           std::vector<dbus::types::Struct<std::tuple<uint32_t, double, uint32_t>>>,
                           std::string, uint32_t, uint32_t
                           >("charge", 0, 50);
            std::for_each(history.value().begin(), history.value().end(), [&as](const dbus::types::Struct<std::tuple<uint32_t, double, uint32_t>>& t)
            {
                as(std::get<1>(t.value));
            });
            std::cout << "Load statistics(Mean: " << acc::mean(as) << ", 2nd Moment: " << std::sqrt(acc::moment<2>(as)) << ")" << std::endl;
        }
        catch (const std::runtime_error& e)
        {
            std::cout << e.what() << std::endl;
        }

        std::cout << "Manual: " << path << std::endl;
        std::cout << "\tVendor: " << device->get_property<org::freedesktop::UPower::Device::Properties::Vendor>()->get() << std::endl;
        std::cout << "\tModel: " << device->get_property<org::freedesktop::UPower::Device::Properties::Model>()->get() << std::endl;
        std::cout << "\tSerial: " << device->get_property<org::freedesktop::UPower::Device::Properties::Serial>()->get() << std::endl;
        std::cout << "\tUpdateTime: " << device->get_property<org::freedesktop::UPower::Device::Properties::UpdateTime>()->get() << std::endl;
        std::cout << "\tType: " << device->get_property<org::freedesktop::UPower::Device::Properties::Type>()->get() << std::endl;
        std::cout << "\tPowerSupply: " << device->get_property<org::freedesktop::UPower::Device::Properties::PowerSupply>()->get() << std::endl;
        std::cout << "\tHasHistory: " << device->get_property<org::freedesktop::UPower::Device::Properties::HasHistory>()->get() << std::endl;
        std::cout << "\tHasStatistics: " << device->get_property<org::freedesktop::UPower::Device::Properties::HasStatistics>()->get() << std::endl;
        std::cout << "\tOnline: " << device->get_property<org::freedesktop::UPower::Device::Properties::Online>()->get() << std::endl;
        std::cout << "\tEnergy: " << device->get_property<org::freedesktop::UPower::Device::Properties::Energy>()->get() << std::endl;
        std::cout << "\tEnergyEmpty: " << device->get_property<org::freedesktop::UPower::Device::Properties::EnergyEmpty>()->get() << std::endl;
        std::cout << "\tEnergyFull: " << device->get_property<org::freedesktop::UPower::Device::Properties::EnergyFull>()->get() << std::endl;
        std::cout << "\tEnergyFullDesign: " << device->get_property<org::freedesktop::UPower::Device::Properties::EnergyFullDesign>()->get() << std::endl;
        std::cout << "\tEnergyRate: " << device->get_property<org::freedesktop::UPower::Device::Properties::EnergyRate>()->get() << std::endl;
        std::cout << "\tVoltage: " << device->get_property<org::freedesktop::UPower::Device::Properties::Voltage>()->get() << std::endl;
        //std::cout << "\tTimeToEmpty: " << device->get_property<UPower::Device::Properties::TimeToEmpty>()->get() << std::endl;
        //std::cout << "\tTimeToFull: " << device->get_property<UPower::Device::Properties::TimeToFull>()->get() << std::endl;
        std::cout << "\tPercentage: " << device->get_property<org::freedesktop::UPower::Device::Properties::Percentage>()->get() << std::endl;
        std::cout << "\tIsPresent: " << device->get_property<org::freedesktop::UPower::Device::Properties::IsPresent>()->get() << std::endl;
        std::cout << "\tState: " << device->get_property<org::freedesktop::UPower::Device::Properties::State>()->get() << std::endl;
        std::cout << "\tIsRechargeable: " << device->get_property<org::freedesktop::UPower::Device::Properties::IsRechargeable>()->get() << std::endl;
        std::cout << "\tCapacity: " << device->get_property<org::freedesktop::UPower::Device::Properties::Capacity>()->get() << std::endl;
        std::cout << "\tTechnology: " << device->get_property<org::freedesktop::UPower::Device::Properties::Technology>()->get() << std::endl;
        std::cout << "\tRecallNotice: " << device->get_property<org::freedesktop::UPower::Device::Properties::RecallNotice>()->get() << std::endl;
        std::cout << "\tRecallVendor: " << device->get_property<org::freedesktop::UPower::Device::Properties::RecallVendor>()->get() << std::endl;
        std::cout << "\tRecallUrl: " << device->get_property<org::freedesktop::UPower::Device::Properties::RecallUrl>()->get() << std::endl;
        std::cout << "-----------------------------------------------------------------------------------" << std::endl;
        auto properties = device->get_all_properties<org::freedesktop::UPower::Device>();
        std::for_each(properties.begin(), properties.end(), [](const std::pair<const std::string, dbus::types::Variant<dbus::types::Any>>& pair)
        {
            std::cout << "\t" << pair.first << std::endl;
        });
        std::cout << "===================================================================================" << std::endl;
    });

    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    int signal;
    sigwait(&signal_set, &signal);

    bus->stop();

    if (t.joinable())
        t.join();
}
