package com.unina.libreria25.service;
import com.unina.libreria25.model.Libro;
import com.unina.libreria25.model.Messaggio;
import com.unina.libreria25.model.Prestito;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;

public class NetworkService {
    private Socket socket;
    private BufferedReader in;
    private PrintWriter out;
    // Attiva/disattiva log di debug
    private static final boolean DEBUG = true;

    private void log(String msg) {
        if (DEBUG)
            System.out.println("[DEBUG] " + msg);
    }

    public void connect(String host, int port) throws IOException {
        if (socket != null && !socket.isClosed()) {
            socket.close();
        }
        socket = new Socket(host, port);
        in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
        out = new PrintWriter(socket.getOutputStream(), true);
        log("Connesso a " + host + ":" + port);
    }

    public String login(String username, String password) throws IOException {
        String cmd = "LOGIN " + username + " " + password;
        out.println(cmd);
        out.flush();
        String resp = safeReadLine();
        log(">> " + cmd + " | << " + resp);
        return resp;
    }

    public String register(String username, String password) throws IOException {
        String cmd = "REGISTER " + username + " " + password;
        out.println(cmd);
        out.flush();
        String resp = safeReadLine();
        log(">> " + cmd + " | << " + resp);
        return resp;
    }

    public ObservableList<Libro> getBooks() throws IOException {
        out.println("LIST_BOOKS");
        out.flush();
        ObservableList<Libro> libri = FXCollections.observableArrayList();
        String line;
        while ((line = in.readLine()) != null && !line.equals("END_LIST")) {
            line = line.trim();
            if (line.isEmpty())
                continue;
            String[] parts = line.split(";");
            if (parts.length < 4)
                continue;
            libri.add(new Libro(Integer.parseInt(parts[0]), parts[1], parts[2], Integer.parseInt(parts[3])));
        }
        log("Ricevuti " + libri.size() + " libri");
        return libri;
    }

    public ObservableList<Prestito> getPrestiti(String username) throws IOException {
        String cmd = "LIST_PRESTITI " + username;
        out.println(cmd);
        out.flush();
        ObservableList<Prestito> lista = FXCollections.observableArrayList();
        String line;
        while ((line = in.readLine()) != null && !line.equals("END_LIST")) {
            line = line.trim();
            if (line.isEmpty())
                continue;
            String[] parts = line.split(";");
            if (parts.length < 6)
                continue;
            lista.add(new Prestito(Integer.parseInt(parts[0]), Integer.parseInt(parts[1]), parts[2], parts[3], parts[4],
                    parts[5]));
        }
        log("Ricevuti " + lista.size() + " prestiti per utente " + username);
        return lista;
    }

    public String addToCart(int codLibro) throws IOException {
        String cmd = "ADD_CART " + codLibro;
        out.println(cmd);
        out.flush();
        String resp = safeReadLine();
        log(">> " + cmd + " | << " + resp);
        return resp;
    }

    public String removeFromCart(int codLibro) throws IOException {
        String cmd = "REMOVE_CART " + codLibro;
        out.println(cmd);
        out.flush();
        String resp = safeReadLine();
        log(">> " + cmd + " | << " + resp);
        return resp;
    }

    public String restituisciLibro(int codLibro) throws IOException {
        String cmd = "RETURN " + codLibro;
        out.println(cmd);
        out.flush();
        String resp = safeReadLine();
        log(">> " + cmd + " | << " + resp);
        return resp;
    }

    public String checkout() throws IOException {
        out.println("CHECKOUT");
        out.flush();
        String resp = safeReadLine();
        log(">> CHECKOUT | << " + resp);
        return resp;
    }

    public void close() {
        try {
            if (socket != null && !socket.isClosed()) {
                out.println("LOGOUT");
                out.flush();
                socket.close();
                log("Connessione chiusa.");
            }
        } catch (IOException e) {
            System.err.println("Errore durante la chiusura della connessione: " + e.getMessage());
        }
    }

    public ObservableList<Integer> getCartCodici() throws IOException {
        out.println("LIST_CART");
        out.flush();
        ObservableList<Integer> codes = FXCollections.observableArrayList();
        String line;
        while ((line = in.readLine()) != null && !line.equals("END_LIST")) {
            line = line.trim();
            if (line.isEmpty())
                continue;
            try {
                codes.add(Integer.parseInt(line));
            } catch (NumberFormatException ex) {
                log("Valore non valido in LIST_CART: " + line);
            }
        }
        log("Ricevuti " + codes.size() + " codici nel carrello");
        return codes;
    }

    // --- METODO DI SICUREZZA: legge una riga e la pulisce ---
    private String safeReadLine() throws IOException {
        String line = in.readLine();
        return (line != null) ? line.trim() : "";
    }

    public String sendMessage(String usernameDest,
            int codPrestito, String testo) throws IOException {

        // Protezione: rimuovi eventuali newline
        testo = testo.replaceAll("[\r\n]", " ").trim();
        String cmd = "SEND_MSG " + usernameDest + " " + codPrestito + " " + testo;
        out.println(cmd);
        out.flush();
        String resp = safeReadLine();
        log(">> " + cmd
                + " | << " + resp);
        return normalizeResponse(resp);
    }

    public String getUtenteDaPrestito(int codPrestito) throws IOException {
        String cmd = "GET_UTENTE_PRESTITO " + codPrestito;
        out.println(cmd);
        out.flush();
        String response = safeReadLine();
        log(">> " + cmd + " | << " + response);
        if (response != null && response.startsWith("OK;")) {
            return response.substring(3);
        } else {
            return null;
        }
    }

    private String normalizeResponse(String resp) {
        if (resp == null)
            return "ERROR";
        resp = resp.trim();
        return switch (resp) {
            case "SEND_MSG_FAIL", "SEND_MSG_FAIL_DB",
                    "SEND_MSG_FAIL_SYNTAX" ->
                "MSG_FAIL";
            case "PERMISSION_DENIED" ->
                "MSG_DENIED";
            case "ERROR_UTENTE_NOT_FOUND" -> "UTENTE_NOT_FOUND";
            default ->
                resp;
        };
    }

    public ObservableList<Prestito> getAllPrestiti() throws IOException {
        out.println("LIST_ALL_PRESTITI");
        out.flush();

        ObservableList<Prestito> lista = FXCollections.observableArrayList();
        String line;
        while ((line = in.readLine()) != null && !line.equals("END_LIST")) {
            line = line.trim();
            if (line.isEmpty())
                continue;
            String[] parts = line.split(";");
            if (parts.length < 9)
                continue;
            Prestito p = new Prestito(
                    Integer.parseInt(parts[0]), Integer.parseInt(parts[1]), parts[2],
                    parts[3], parts[7], parts[8]);
            p.setCopieDisponibili(Integer.parseInt(parts[4]));
            p.setCopieInPrestito(Integer.parseInt(parts[5]));
            p.setUtente(parts[6]);
            lista.add(p);
        }
        return lista;
    }

    public ObservableList<Messaggio> getMessages(String username) throws IOException {
        out.println("LIST_MSG " +
                username);
        out.flush();
        ObservableList<Messaggio> lista = FXCollections.observableArrayList();
        String line;
        while ((line = in.readLine()) != null && !line.equals("END_LIST")) {
            line = line.trim();
            if (line.isEmpty())
                continue;
            String[] parts = line.split(";");
            if (parts.length < 4)
                continue;
            int codMsg = Integer.parseInt(parts[0]);
            int codPrestito = Integer.parseInt(parts[1]);
            String testo = parts[2];
            String data = parts[3];
            lista.add(new Messaggio(codMsg, codPrestito, testo, data));
        }
        return lista;
    }

    public String deleteMessage(int codMsg) throws IOException {
        out.println("DELETE_MSG " + codMsg);
        out.flush();
        return safeReadLine();
    }
}
