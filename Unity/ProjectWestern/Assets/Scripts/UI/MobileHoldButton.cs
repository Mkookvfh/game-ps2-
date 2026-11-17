
using UnityEngine;
using UnityEngine.EventSystems;

public sealed class MobileHoldButton : MonoBehaviour,
    IPointerDownHandler, IPointerUpHandler
{
    [SerializeField] private ThirdPersonController player;

    public void OnPointerDown(PointerEventData eventData)
    {
        if (player != null)
            player.SetSprint(true);
    }

    public void OnPointerUp(PointerEventData eventData)
    {
        if (player != null)
            player.SetSprint(false);
    }

    private void OnDisable()
    {
        if (player != null)
            player.SetSprint(false);
    }
}
