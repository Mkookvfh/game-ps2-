
using UnityEngine;
using UnityEngine.EventSystems;

public sealed class PlayerMobileInput : MonoBehaviour,
    IPointerDownHandler, IDragHandler, IPointerUpHandler
{
    [SerializeField] private RectTransform joystickArea;
    [SerializeField] private RectTransform joystickHandle;
    [SerializeField, Min(1f)] private float handleRange = 70f;

    public Vector2 MoveInput { get; private set; }

    public void OnPointerDown(PointerEventData eventData)
        => UpdateJoystick(eventData);

    public void OnDrag(PointerEventData eventData)
        => UpdateJoystick(eventData);

    public void OnPointerUp(PointerEventData eventData)
    {
        MoveInput = Vector2.zero;

        if (joystickHandle != null)
            joystickHandle.anchoredPosition = Vector2.zero;
    }

    private void UpdateJoystick(PointerEventData eventData)
    {
        if (joystickArea == null) return;

        if (!RectTransformUtility.ScreenPointToLocalPointInRectangle(
            joystickArea,
            eventData.position,
            eventData.pressEventCamera,
            out Vector2 localPoint))
            return;

        Vector2 normalized =
            Vector2.ClampMagnitude(localPoint / handleRange, 1f);

        MoveInput = normalized;

        if (joystickHandle != null)
            joystickHandle.anchoredPosition = normalized * handleRange;
    }

    private void OnDisable()
    {
        MoveInput = Vector2.zero;

        if (joystickHandle != null)
            joystickHandle.anchoredPosition = Vector2.zero;
    }
}
