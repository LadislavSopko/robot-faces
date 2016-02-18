import QtQuick 2.0

SequentialAnimation {
    id: root

    property var face

    ScriptAction {
        script: {
            face.mouth.visibleRangeMin = -1;
            face.mouth.visibleRangeMax = -1;
            face.mouth.block.visible = true;
        }
    }

    ParallelAnimation {
        ColorAnimation {
            targets: [face.leftEye.border, face.rightEye.border, face.nose]
            property: "color"
            from: face.leftEye.restingBorderColor
            to: "white"
            duration: 400
        }
        MovePupilsToCenterAnimation {
            face: root.face
        }
    }

}
