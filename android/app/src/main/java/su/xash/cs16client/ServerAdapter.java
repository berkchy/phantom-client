package su.xash.cs16client;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

public class ServerAdapter extends RecyclerView.Adapter<ServerAdapter.Holder> {
    public interface Listener {
        void onServerClick(ServerEntry entry);
    }

    private final List<ServerEntry> entries = new ArrayList<>();
    private final Listener listener;

    public ServerAdapter(Listener listener) {
        this.listener = listener;
    }

    public void submit(List<ServerEntry> newEntries) {
        entries.clear();
        if (newEntries != null) {
            entries.addAll(newEntries);
        }
        notifyDataSetChanged();
    }

    public ServerEntry getItem(int position) {
        if (position < 0 || position >= entries.size()) {
            return null;
        }
        return entries.get(position);
    }

    @NonNull
    @Override
    public Holder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_server, parent, false);
        return new Holder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull Holder holder, int position) {
        ServerEntry entry = entries.get(position);
        holder.name.setText(entry.name == null || entry.name.isEmpty() ? entry.address : entry.name);
        holder.details.setText(entry.detailsText());
        holder.players.setText(entry.playersText());
        holder.ping.setText(entry.pingMs > 0 ? entry.pingMs + " ms" : "---");
        holder.itemView.setOnClickListener(v -> {
            if (listener != null) {
                listener.onServerClick(entry);
            }
        });
    }

    @Override
    public int getItemCount() {
        return entries.size();
    }

    static class Holder extends RecyclerView.ViewHolder {
        final TextView name;
        final TextView details;
        final TextView players;
        final TextView ping;

        Holder(@NonNull View itemView) {
            super(itemView);
            name = itemView.findViewById(R.id.serverName);
            details = itemView.findViewById(R.id.serverDetails);
            players = itemView.findViewById(R.id.serverPlayers);
            ping = itemView.findViewById(R.id.serverPing);
        }
    }
}
