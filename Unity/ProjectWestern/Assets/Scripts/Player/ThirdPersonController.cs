
using UnityEngine;

[RequireComponent(typeof(CharacterController))]
public sealed class ThirdPersonController : MonoBehaviour
{
    [Header("References")]
    [SerializeField] private PlayerMobileInput mobileInput;
    [SerializeField] private Transform cameraTransform;
    [SerializeField] private Animator animator;

    [Header("Movement")]
    [SerializeField] private float walkSpeed = 2.5f;
    [SerializeField] private float runSpeed = 5f;
    [SerializeField] private float rotationSmoothTime = 0.12f;

    [Header("Jump and gravity")]
    [SerializeField] private float jumpHeight = 1.2f;
    [SerializeField] private float gravity = -20f;

    private CharacterController characterController;
    private Vector3 verticalVelocity;
    private float rotationVelocity;
    private bool sprintHeld;
    private bool jumpRequested;

    private void Awake()
    {
        characterController = GetComponent<CharacterController>();

        if (cameraTransform == null && Camera.main != null)
            cameraTransform = Camera.main.transform;
    }

    private void Update()
    {
        Move();
        ApplyJumpAndGravity();
        UpdateAnimator();
    }

    private void Move()
    {
        if (mobileInput == null || cameraTransform == null) return;

        Vector2 input = Vector2.ClampMagnitude(
            mobileInput.MoveInput, 1f);

        Vector3 forward = cameraTransform.forward;
        Vector3 right = cameraTransform.right;

        forward.y = 0f;
        right.y = 0f;
        forward.Normalize();
        right.Normalize();

        Vector3 direction = forward * input.y + right * input.x;

        if (direction.sqrMagnitude > 0.001f)
        {
            float targetAngle =
                Mathf.Atan2(direction.x, direction.z) * Mathf.Rad2Deg;

            float angle = Mathf.SmoothDampAngle(
                transform.eulerAngles.y,
                targetAngle,
                ref rotationVelocity,
                rotationSmoothTime);

            transform.rotation = Quaternion.Euler(0f, angle, 0f);
        }

        float speed = sprintHeld ? runSpeed : walkSpeed;

        characterController.Move(
            direction.normalized * speed * Time.deltaTime);
    }

    private void ApplyJumpAndGravity()
    {
        if (characterController.isGrounded && verticalVelocity.y < 0f)
            verticalVelocity.y = -2f;

        if (jumpRequested && characterController.isGrounded)
        {
            verticalVelocity.y =
                Mathf.Sqrt(jumpHeight * -2f * gravity);

            if (animator != null)
                animator.SetTrigger("Jump");
        }

        jumpRequested = false;

        verticalVelocity.y += gravity * Time.deltaTime;

        characterController.Move(
            verticalVelocity * Time.deltaTime);
    }

    private void UpdateAnimator()
    {
        if (animator == null || mobileInput == null) return;

        float amount = mobileInput.MoveInput.magnitude;

        animator.SetFloat(
            "Speed", amount * (sprintHeld ? 2f : 1f));
    }

    public void SetSprint(bool held)
    {
        sprintHeld = held;
    }

    public void RequestJump()
    {
        jumpRequested = true;
    }

    private void OnDisable()
    {
        sprintHeld = false;
        jumpRequested = false;
    }
}
