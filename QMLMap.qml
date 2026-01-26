import QtQuick 2.12
import QtQuick.Window 2.12
import QtLocation 5.12
import QtPositioning 5.12

Item {
    width: 512
    height: 512
    visible: true
//    Plugin {
//        id: mapPlugin
//        name: "osm" // "mapboxgl", "esri", ...
//        // specify plugin parameters if necessary
//        // PluginParameter {
//        //     name:
//        //     value:
//        // }
//    }

    Map {
        id: map
        anchors.fill: parent
        copyrightsVisible: false
//        activeMapType: map.supportedMapTypes[1]
        zoomLevel: 1
        plugin: Plugin {
            name: 'googlemaps';
           // PluginParameter {
           //     name: 'osm.mapping.offline.directory'
           //     value: ':/offline_tiles/'
           // }
        }
        center: QtPositioning.coordinate(13.739957, 100.749210) // Oslo
    }
}
