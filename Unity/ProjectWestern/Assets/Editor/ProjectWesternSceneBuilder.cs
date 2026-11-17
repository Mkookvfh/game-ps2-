
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class ProjectWesternSceneBuilder
{
    private const string SceneFolder =
        "Assets/Scenes";

    private const string ScenePath =
        "Assets/Scenes/WesternWorld.unity";

    [MenuItem("Project Western/Build Starter Scene")]
    public static void BuildStarterScene()
    {
        if (!AssetDatabase.IsValidFolder(SceneFolder))
        {
            AssetDatabase.CreateFolder("Assets", "Scenes");
        }

        var scene = EditorSceneManager.NewScene(
            NewSceneSetup.EmptyScene,
            NewSceneMode.Single
        );

        var ground = GameObject.CreatePrimitive(
            PrimitiveType.Plane
        );
        ground.name = "World_Ground";
        ground.transform.localScale =
            new Vector3(10f, 1f, 10f);

        var player = new GameObject("Player");
        player.transform.position =
            new Vector3(0f, 1f, 0f);

        var controller =
            player.AddComponent<CharacterController>();

        controller.height = 2f;
        controller.radius = 0.4f;
        controller.center = new Vector3(0f, 0f, 0f);

        var cameraObject = new GameObject("Main Camera");
        cameraObject.tag = "MainCamera";
        cameraObject.transform.position =
            new Vector3(0f, 5f, -7f);
        cameraObject.transform.rotation =
            Quaternion.Euler(25f, 0f, 0f);

        cameraObject.AddComponent<Camera>();
        cameraObject.AddComponent<AudioListener>();

        var lightObject = new GameObject("Directional Light");
        var light = lightObject.AddComponent<Light>();
        light.type = LightType.Directional;
        lightObject.transform.rotation =
            Quaternion.Euler(50f, -30f, 0f);

        EditorSceneManager.SaveScene(scene, ScenePath);
        AssetDatabase.SaveAssets();

        Debug.Log("Project Western starter scene created.");
    }
}
