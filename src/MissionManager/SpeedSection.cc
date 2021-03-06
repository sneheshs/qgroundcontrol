/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SpeedSection.h"
#include "JsonHelper.h"
#include "FirmwarePlugin.h"
#include "SimpleMissionItem.h"

const char* SpeedSection::_flightSpeedName = "FlightSpeed";

QMap<QString, FactMetaData*> SpeedSection::_metaDataMap;

SpeedSection::SpeedSection(Vehicle* vehicle, QObject* parent)
    : Section               (vehicle, parent)
    , _available            (false)
    , _dirty                (false)
    , _specifyFlightSpeed   (false)
    , _flightSpeedFact      (0, _flightSpeedName,   FactMetaData::valueTypeDouble)
{
    if (_metaDataMap.isEmpty()) {
        _metaDataMap = FactMetaData::createMapFromJsonFile(QStringLiteral(":/json/SpeedSection.FactMetaData.json"), NULL /* metaDataParent */);
    }

    double hoverSpeed, cruiseSpeed;
    double flightSpeed = 0;

    _vehicle->firmwarePlugin()->missionFlightSpeedInfo(_vehicle, hoverSpeed, cruiseSpeed);
    if (_vehicle->multiRotor()) {
        flightSpeed = hoverSpeed;
    } else if (_vehicle->fixedWing()) {
        flightSpeed = cruiseSpeed;
    }

    _metaDataMap[_flightSpeedName]->setRawDefaultValue(flightSpeed);
    _flightSpeedFact.setMetaData(_metaDataMap[_flightSpeedName]);
    _flightSpeedFact.setRawValue(flightSpeed);

    connect(this,               &SpeedSection::specifyFlightSpeedChanged,  this, &SpeedSection::_setDirtyAndUpdateItemCount);
    connect(this,               &SpeedSection::specifyFlightSpeedChanged,  this, &SpeedSection::settingsSpecifiedChanged);
    connect(&_flightSpeedFact,  &Fact::valueChanged,                       this, &SpeedSection::_setDirty);
}

bool SpeedSection::settingsSpecified(void) const
{
    return _specifyFlightSpeed;
}

void SpeedSection::setAvailable(bool available)
{
    if (available != _available) {
        if (available && (_vehicle->multiRotor() || _vehicle->fixedWing())) {
            _available = available;
            emit availableChanged(available);
        }
    }
}

void SpeedSection::_setDirty(void)
{
    setDirty(true);
}

void SpeedSection::_setDirtyAndUpdateItemCount(void)
{
    setDirty(true);
    emit itemCountChanged(itemCount());
}

void SpeedSection::setDirty(bool dirty)
{
    if (_dirty != dirty) {
        _dirty = dirty;
        emit dirtyChanged(_dirty);
    }
}

void SpeedSection::setSpecifyFlightSpeed(bool specifyFlightSpeed)
{
    if (specifyFlightSpeed != _specifyFlightSpeed) {
        _specifyFlightSpeed = specifyFlightSpeed;
        emit specifyFlightSpeedChanged(specifyFlightSpeed);
    }
}

int SpeedSection::itemCount(void) const
{
    return _specifyFlightSpeed ? 1: 0;
}

void SpeedSection::appendSectionItems(QList<MissionItem*>& items, QObject* missionItemParent, int& seqNum)
{
    // IMPORTANT NOTE: If anything changes here you must also change SpeedSection::scanForSettings

    if (_specifyFlightSpeed) {
        MissionItem* item = new MissionItem(seqNum++,
                                            MAV_CMD_DO_CHANGE_SPEED,
                                            MAV_FRAME_MISSION,
                                            _vehicle->multiRotor() ? 1 /* groundspeed */ : 0 /* airspeed */,    // Change airspeed or groundspeed
                                            _flightSpeedFact.rawValue().toDouble(),
                                            -1,                                                                 // No throttle change
                                            0,                                                                  // Absolute speed change
                                            0, 0, 0,                                                            // param 5-7 not used
                                            true,                                                               // autoContinue
                                            false,                                                              // isCurrentItem
                                            missionItemParent);
        items.append(item);
    }
}

bool SpeedSection::scanForSection(QmlObjectListModel* visualItems, int& scanIndex)
{
    if (!_available || scanIndex >= visualItems->count()) {
        return false;
    }

    SimpleMissionItem* item = visualItems->value<SimpleMissionItem*>(scanIndex);
    if (!item) {
        // We hit a complex item, there can't be a speed setting
        return false;
    }
    MissionItem& missionItem = item->missionItem();

    // See SpeedSection::appendMissionItems for specs on what consitutes a known speed setting

    if (missionItem.command() == MAV_CMD_DO_CHANGE_SPEED && missionItem.param3() == -1 && missionItem.param4() == 0 && missionItem.param5() == 0 && missionItem.param6() == 0 && missionItem.param7() == 0) {
        if (_vehicle->multiRotor() && missionItem.param1() != 1) {
            return false;
        } else if (_vehicle->fixedWing() && missionItem.param1() != 0) {
            return false;
        }
        visualItems->removeAt(scanIndex)->deleteLater();
        _flightSpeedFact.setRawValue(missionItem.param2());
        setSpecifyFlightSpeed(true);
        scanIndex++;
        return true;
    }

    return false;
}
