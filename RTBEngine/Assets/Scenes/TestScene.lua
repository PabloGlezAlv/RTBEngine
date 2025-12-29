function CreateScene()
    return {
        name = "Test Scene",
        gameObjects = {
            -- Main Camera
            {
                name = "MainCamera",
                position = Vector3(0.0, 1.0, 5.0),
                rotation = Quaternion.FromEulerAngles(0.0, 180.0, 0.0),
                components = {
                    {
                        type = "CameraComponent",
                        fov = 45.0,
                        nearPlane = 0.1,
                        farPlane = 1000.0,
                        projection = "Perspective",
                        isMain = true
                    },
                    {
                        type = "FreeLookCamera",
                        moveSpeed = 10.0,
                        lookSpeed = 0.2
                    }
                }
            },
            -- Directional Light
            {
                name = "MainLight",
                rotation = Quaternion.FromEulerAngles(45.0, 45.0, 0.0),
                components = {
                    {
                        type = "LightComponent",
                        lightType = "Directional",
                        color = Vector3(1.0, 1.0, 1.0),
                        intensity = 1.0,
                        castShadows = true,
                        shadowMapResolution = 2048,
                        shadowBias = 0.005
                    }

                }
            },
            -- Point Light
            {
                name = "PointLight",
                position = Vector3(3.0, 4.0, 0.0),
                components = {
                    {
                        type = "LightComponent",
                        lightType = "Point",
                        color = Vector3(0.2, 0.5, 1.0),
                        intensity = 1.0,
                        range = 30.0
                    }
                }
            },
            -- Spot Light
            {
                name = "SpotLight",
                position = Vector3(-3.0, 6.0, 0.0),
                rotation = Quaternion.FromEulerAngles(90.0, 0.0, 0.0),
                components = {
                    {
                        type = "LightComponent",
                        lightType = "Spot",
                        color = Vector3(1.0, 0.0, 0.0),
                        intensity = 50.0,
                        range = 50.0,
                        innerCutOff = 5.0,
                        outerCutOff = 15.0
                    }
                }
            },
            -- UI Canvas
            {
                name = "UICanvas",
                components = {
                    {
                        type = "Canvas",
                        sortOrder = 0
                    }
                }
            },
            -- Title Text
            {
                name = "TitleText",
                parent = "UICanvas",
                components = {
                    {
                        type = "UIText",
                        text = "RTBEngine",
                        color = Vector4(1.0, 1.0, 1.0, 1.0),
                        fontSize = 24.0,
                        alignment = 1, -- Center
                        -- RectTransform
                        anchorMin = Vector2(0.0, 1.0),
                        anchorMax = Vector2(0.0, 1.0),
                        pivot = Vector2(0.0, 1.0),
                        anchoredPosition = Vector2(10.0, -10.0),
                        sizeDelta = Vector2(200.0, 50.0)
                    }
                }
            },
            -- Logo Image
            {
                name = "LogoImage",
                parent = "UICanvas",
                components = {
                    {
                        type = "UIImage",
                        texture = "Default/Textures/logo.png",
                        color = Vector4(1.0, 1.0, 1.0, 1.0),
                        preserveAspect = true,
                        -- RectTransform
                        anchorMin = Vector2(1.0, 1.0),
                        anchorMax = Vector2(1.0, 1.0),
                        pivot = Vector2(1.0, 1.0),
                        anchoredPosition = Vector2(-10.0, -10.0),
                        sizeDelta = Vector2(64.0, 64.0)
                    }
                }
            },
            -- Panel Button
            {
                name = "PanelButton",
                parent = "UICanvas",
                components = {
                    {
                        type = "UIPanel",
                        color = Vector4(0.2, 0.6, 0.3, 1.0),
                        borderColor = Vector4(1.0, 1.0, 1.0, 1.0),
                        borderThickness = 2.0,
                        hasBorder = true,
                        -- RectTransform
                        anchorMin = Vector2(0.0, 0.5),
                        anchorMax = Vector2(0.0, 0.5),
                        pivot = Vector2(0.0, 0.5),
                        anchoredPosition = Vector2(20.0, 0.0),
                        sizeDelta = Vector2(150.0, 50.0)
                    },
                    {
                        type = "UIButton",
                        normalColor = Vector4(1.0, 1.0, 1.0, 1.0),
                        hoveredColor = Vector4(1.3, 1.3, 1.3, 1.0),
                        pressedColor = Vector4(0.7, 0.7, 0.7, 1.0)
                    }
                }
            },
            -- Floor
            {
                name = "Floor",
                position = Vector3(0.0, -0.05, 0.0),
                scale = Vector3(10.0, 0.1, 10.0),
                components = {
                    {
                        type = "MeshRenderer",
                        mesh = "Default/Models/cube.obj",
                        shader = "basic",
                    }
                }
            },
            -- Static Cube (shadow caster)
            {
                name = "Cube",
                position = Vector3(2.0, 1.0, 0.0),
                scale = Vector3(1.0, 1.0, 1.0),
                components = {
                    {
                        type = "MeshRenderer",
                        mesh = "Default/Models/cube.obj",
                        shader = "basic"
                    }
                }
            },
            -- Animated Character
            {
                name = "Character",
                position = Vector3(0.0, 0.0, 0.0),
                scale = Vector3(0.01, 0.01, 0.01),
                components = {
                    {
                        type = "MeshRenderer",
                        shader = "basic",
                    },
                    {
                        type = "Animator",
                        model = "Assets/Models/walking.fbx",
                        defaultClip = "mixamo.com",
                        loop = true,
                        speed = 1.0
                    }
                }
            }
        }
    }
end
