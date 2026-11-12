#ifndef SAVED_NETWORKS_RUNTIME_H_
#define SAVED_NETWORKS_RUNTIME_H_

// Settings -> Saved Wi-Fi (#43): the networks Keepsake knows, most recent first. Picking one
// asks "Forget this Wi-Fi?"; Forget removes it (wifi_service::ForgetNetwork).
namespace saved_networks_runtime {

// Opens the list (a select modal), or says there's nothing saved yet. Call outside input
// dispatch.
void RequestShowList();
// app_shell polls this after input dispatch; returns true if it showed something.
bool ShowListIfRequested();
// A select-modal submission: true if it was this list's (it then asks to confirm).
bool HandleSelection(int selected_index);
// "Forget" on the confirmation.
void ForgetConfirmed();

}  // namespace saved_networks_runtime

#endif  // SAVED_NETWORKS_RUNTIME_H_
