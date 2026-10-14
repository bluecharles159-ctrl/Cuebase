# Set up Cuebase's account service

You do not need Code::Blocks for this. These steps prepare the online account
service; they do not mean desktop sign-in or synchronization is already working.

1. Open [Supabase](https://supabase.com/dashboard) and sign in to your own account.
2. Create a project named **Cuebase**. Keep the database password in your password manager.
3. Wait for the project to become ready, then open its **Connect** dialog.
4. Copy the **Project URL** and **publishable key**. The key starts with `sb_publishable_`.
5. Share those two public configuration values with the developer when ready.
   Keep the database password, secret keys and OAuth client secrets private.

Supabase documents the Connect dialog and key types in its
[API key guide](https://supabase.com/docs/guides/getting-started/api-keys).

Next, we need to configure the prepared database migration, email delivery,
Google/GitHub providers and the desktop callback. The detailed checklist is
[SETUP.md](SETUP.md). Do not guess a callback address: use the address selected
and tested by the desktop implementation.

The completion test is signing in on two devices, seeing the same authorized
channel, saving and reopening changes, and confirming that unrelated accounts
cannot access its data. That test has not yet been completed.
