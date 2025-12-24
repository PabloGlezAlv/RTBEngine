function CreateScene()
    return {
        name = "TestScene",
        gameObjects = {
            {
                name = "TestCube",
                position = Vector3(0.0, 0.0, 0.0),
                rotation = Quaternion.FromEulerAngles(0.0, 0.0, 0.0),
                scale = Vector3(1.0, 1.0, 1.0),
                components = {
                    { type = "MeshRenderer" }
                }
            },
            {
                name = "MainLight",
                position = Vector3(0.0, 5.0, 0.0),
                rotation = Quaternion.FromEulerAngles(0.0, 0.0, 0.0),
                scale = Vector3(1.0, 1.0, 1.0),
                components = {
                    { type = "LightComponent" }
                }
            }
        }
    }
end
