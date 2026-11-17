
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;
using UnityEngine.EventSystems;
using UnityEngine.Serialization;
using UnityEngine.InputSystem.UI;

public static class ProjectWesternSceneBuilder
{
    private const string SceneFolder = "Assets/Scenes";
    private const string ScenePath = "Assets/Scenes/WesternWorld.unity";

    [MenuItem("Project Western/Build Starter Scene")]
    public static void BuildStarterScene()
    {
        if (!AssetDatabase.IsValidFolder(SceneFolder))
            AssetDatabase.CreateFolder("Assets", "Scenes");

        Scene scene = EditorSceneManager.NewScene(
            NewSceneSetup.EmptyScene,
            NewSceneMode.Single
        );

        // Ground
        GameObject ground = GameObject.CreatePrimitive(
            PrimitiveType.Plane
        );
        ground.name = "World_Ground";
        ground.transform.localScale = new Vector3(10f, 1f, 10f);

        // Player
        GameObject player = new GameObject("Player");
        player.transform.position = new Vector3(0f, 1f, 0f);

        CharacterController character =
            player.AddComponent<CharacterController>();

        character.height = 2f;
        character.radius = 0.4f;
        character.center = Vector3.zero;

        ThirdPersonController movement =
            player.AddComponent<ThirdPersonController>();


// Temporary visible player body
GameObject playerBody = GameObject.CreatePrimitive(
    PrimitiveType.Capsule
);

playerBody.name = "PlayerBody";
playerBody.transform.SetParent(player.transform, false);
playerBody.transform.localPosition = Vector3.zero;
playerBody.transform.localScale = new Vector3(0.8f, 1f, 0.8f);

// Remove the extra collider because Player already has CharacterController
Collider bodyCollider = playerBody.GetComponent<Collider>();

if (bodyCollider != null)
    Object.DestroyImmediate(bodyCollider);

        // Camera
        GameObject cameraObject = new GameObject("Main Camera");
        cameraObject.tag = "MainCamera";
        cameraObject.transform.SetParent(player.transform, false);
        cameraObject.transform.localPosition = new Vector3(0f, 5f, -7f);
        cameraObject.transform.localRotation = Quaternion.Euler(25f, 0f, 0f);

        cameraObject.AddComponent<Camera>();
        cameraObject.AddComponent<AudioListener>();

        // Directional Light
        GameObject lightObject = new GameObject("Directional Light");
        Light light = lightObject.AddComponent<Light>();
        light.type = LightType.Directional;
        lightObject.transform.rotation = Quaternion.Euler(50f, -30f, 0f);

        // Canvas
        GameObject canvasObject = new GameObject(
            "MobileCanvas",
            typeof(Canvas),
            typeof(CanvasScaler),
            typeof(GraphicRaycaster)
        );

        Canvas canvas = canvasObject.GetComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;

        CanvasScaler scaler = canvasObject.GetComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920f, 1080f);

        // Event System
        GameObject eventObject = new GameObject(
            "EventSystem",
            typeof(EventSystem),
            typeof(InputSystemUIInputModule)
        );

        // Joystick Base
        GameObject joystickBase = CreateUIElement(
            "JoystickBase",
            canvasObject.transform,
            new Vector2(0f, 0f),
            new Vector2(0f, 0f),
            new Vector2(170f, 170f)
        );

        Image baseImage = joystickBase.AddComponent<Image>();
        baseImage.color = new Color(0.15f, 0.15f, 0.15f, 0.65f);

        // Joystick Handle
        GameObject joystickHandle = CreateUIElement(
            "JoystickHandle",
            joystickBase.transform,
            new Vector2(0.5f, 0.5f),
            new Vector2(0.5f, 0.5f),
            new Vector2(70f, 70f)
        );

        Image handleImage = joystickHandle.AddComponent<Image>();
        handleImage.color = new Color(0.8f, 0.8f, 0.8f, 0.9f);
        handleImage.raycastTarget = false;

        PlayerMobileInput mobileInput =
            joystickBase.AddComponent<PlayerMobileInput>();

        SerializedObject mobileSerialized =
            new SerializedObject(mobileInput);

        mobileSerialized.FindProperty("joystickArea").objectReferenceValue =
            joystickBase.GetComponent<RectTransform>();

        mobileSerialized.FindProperty("joystickHandle").objectReferenceValue =
            joystickHandle.GetComponent<RectTransform>();

        mobileSerialized.ApplyModifiedPropertiesWithoutUndo();

        // Sprint Button
        GameObject sprintButton = CreateUIElement(
            "SprintButton",
            canvasObject.transform,
            new Vector2(1f, 0f),
            new Vector2(1f, 0f),
            new Vector2(170f, 100f)
        );

        Image sprintImage = sprintButton.AddComponent<Image>();
        sprintImage.color = new Color(0.65f, 0.3f, 0.1f, 0.85f);

GameObject sprintTextObject = new GameObject(
    "SprintLabel",
    typeof(RectTransform)
);

sprintTextObject.transform.SetParent(sprintButton.transform, false);

RectTransform sprintTextRect =
    sprintTextObject.GetComponent<RectTransform>();

sprintTextRect.anchorMin = Vector2.zero;
sprintTextRect.anchorMax = Vector2.one;
sprintTextRect.offsetMin = Vector2.zero;
sprintTextRect.offsetMax = Vector2.zero;

Text sprintText = sprintTextObject.AddComponent<Text>();
sprintText.text = "SPRINT";
sprintText.alignment = TextAnchor.MiddleCenter;
sprintText.font = Resources.GetBuiltinResource<Font>("Arial.ttf");
sprintText.fontSize = 28;
sprintText.color = Color.white;
sprintText.raycastTarget = false;


        MobileHoldButton holdButton =
            sprintButton.AddComponent<MobileHoldButton>();

        SerializedObject holdSerialized =
            new SerializedObject(holdButton);

        holdSerialized.FindProperty("player").objectReferenceValue =
            movement;

        holdSerialized.ApplyModifiedPropertiesWithoutUndo();

        // Connect Player Controller
        SerializedObject movementSerialized =
            new SerializedObject(movement);

        movementSerialized.FindProperty("mobileInput").objectReferenceValue =
            mobileInput;

        movementSerialized.FindProperty("cameraTransform").objectReferenceValue =
            cameraObject.transform;

        movementSerialized.ApplyModifiedPropertiesWithoutUndo();

        // Save Scene
        EditorSceneManager.SaveScene(scene, ScenePath);
        AssetDatabase.SaveAssets();

        Debug.Log("Project Western starter scene created successfully.");
    }

    private static GameObject CreateUIElement(
        string objectName,
        Transform parent,
        Vector2 anchor,
        Vector2 pivot,
        Vector2 size
    )
    {
        GameObject obj = new GameObject(
            objectName,
            typeof(RectTransform)
        );

        obj.transform.SetParent(parent, false);

        RectTransform rect = obj.GetComponent<RectTransform>();
        rect.anchorMin = anchor;
        rect.anchorMax = anchor;
        rect.pivot = pivot;
        rect.sizeDelta = size;

        if (objectName == "JoystickBase")
            rect.anchoredPosition = new Vector2(150f, 150f);
        else if (objectName == "SprintButton")
            rect.anchoredPosition = new Vector2(-150f, 150f);

        return obj;
    }
}
