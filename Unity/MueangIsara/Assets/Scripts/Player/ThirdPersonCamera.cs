
using UnityEngine;

public sealed class ThirdPersonCamera : MonoBehaviour
{
    [Header("Target")]
    [SerializeField] private Transform target;

    [Header("Camera Settings")]
    [SerializeField] private float distance = 4.5f;
    [SerializeField] private float targetHeight = 1.5f;
    [SerializeField] private float sensitivity = 3f;

    [Header("Camera Rotation")]
    [SerializeField] private float minPitch = -25f;
    [SerializeField] private float maxPitch = 65f;

    [Header("Smoothing")]
    [SerializeField] private float followSmoothness = 12f;

    [Header("Collision")]
    [SerializeField] private float collisionRadius = 0.2f;
    [SerializeField] private float minimumDistance = 0.5f;

    private float yaw;
    private float pitch = 15f;

    private void Start()
    {
        if (target != null)
            yaw = target.eulerAngles.y;
    }

    private void LateUpdate()
    {
        if (target == null)
            return;

        UpdateRotation();
        UpdatePosition();
    }

    private void UpdateRotation()
    {
        yaw += Input.GetAxis("Mouse X") * sensitivity;
        pitch -= Input.GetAxis("Mouse Y") * sensitivity;

        pitch = Mathf.Clamp(
            pitch,
            minPitch,
            maxPitch
        );
    }

    private void UpdatePosition()
    {
        Quaternion rotation = Quaternion.Euler(
            pitch, yaw, 0f
        );

        Vector3 focusPoint =
            target.position + Vector3.up * targetHeight;

        Vector3 direction = rotation * Vector3.back;

        float currentDistance = distance;

        if (Physics.SphereCast(
            focusPoint,
            collisionRadius,
            direction,
            out RaycastHit hit,
            distance
        ))
        {
            currentDistance = Mathf.Max(
                minimumDistance,
                hit.distance - collisionRadius
            );
        }

        Vector3 desiredPosition =
            focusPoint + direction * currentDistance;

        float blend = 1f - Mathf.Exp(
            -followSmoothness * Time.deltaTime
        );

        transform.position = Vector3.Lerp(
            transform.position,
            desiredPosition,
            blend
        );

        transform.rotation = Quaternion.LookRotation(
            focusPoint - transform.position,
            Vector3.up
        );
    }

    public void SetTarget(Transform newTarget)
    {
        target = newTarget;
    }
}
