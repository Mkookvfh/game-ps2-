
using UnityEngine;

[RequireComponent(typeof(CharacterController))]
public sealed class ThirdPersonController : MonoBehaviour
{
    [Header("Camera")]
    [SerializeField] private Transform cameraTransform;

    [Header("Movement")]
    [SerializeField] private float walkSpeed = 3.5f;
    [SerializeField] private float sprintSpeed = 6f;
    [SerializeField] private float rotationSpeed = 12f;

    [Header("Jump and Gravity")]
    [SerializeField] private float jumpHeight = 1.2f;
    [SerializeField] private float gravity = -20f;

    private CharacterController controller;
    private Vector3 verticalVelocity;

    private void Awake()
    {
        controller = GetComponent<CharacterController>();

        if (cameraTransform == null && Camera.main != null)
            cameraTransform = Camera.main.transform;
    }

    private void Update()
    {
        Move();
        ApplyGravity();
    }

    private void Move()
    {
        float x = Input.GetAxisRaw("Horizontal");
        float z = Input.GetAxisRaw("Vertical");

        Vector3 input = Vector3.ClampMagnitude(
            new Vector3(x, 0f, z), 1f
        );

        Vector3 forward = transform.forward;
        Vector3 right = transform.right;

        if (cameraTransform != null)
        {
            forward = Vector3.ProjectOnPlane(
                cameraTransform.forward, Vector3.up
            ).normalized;

            right = Vector3.ProjectOnPlane(
                cameraTransform.right, Vector3.up
            ).normalized;
        }

        Vector3 direction =
            (forward * input.z + right * input.x).normalized;

        bool sprinting = Input.GetKey(KeyCode.LeftShift);

        float speed = sprinting ? sprintSpeed : walkSpeed;

        if (direction.sqrMagnitude > 0.001f)
        {
            Quaternion targetRotation =
                Quaternion.LookRotation(direction, Vector3.up);

            transform.rotation = Quaternion.Slerp(
                transform.rotation,
                targetRotation,
                rotationSpeed * Time.deltaTime
            );
        }

        Vector3 horizontalVelocity = direction * speed;

        controller.Move(
            (horizontalVelocity + verticalVelocity) * Time.deltaTime
        );
    }

    private void ApplyGravity()
    {
        if (controller.isGrounded && verticalVelocity.y < 0f)
            verticalVelocity.y = -2f;

        if (controller.isGrounded &&
            Input.GetButtonDown("Jump"))
        {
            verticalVelocity.y =
                Mathf.Sqrt(jumpHeight * -2f * gravity);
        }

        verticalVelocity.y += gravity * Time.deltaTime;
    }

    public void SetCamera(Transform cameraTarget)
    {
        cameraTransform = cameraTarget;
    }
}
