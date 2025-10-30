package com.unina.libreria25.model;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.time.LocalDate;

public class Prestito {
    private int codPrestito;
    private int codLibro;
    private String titolo;
    private String autore;
    private String dataInizio;
    private String dataFine;
    private String utente;

    // Nuovi campi per la vista del libraio
    private int copieDisponibili;
    private int copieInPrestito;


    // 🔹 Costruttori

    // Costruttore base per prestiti utente
    public Prestito(int codPrestito, int codLibro, String titolo, String autore, String dataInizio, String dataFine) {
        this.codPrestito = codPrestito;
        this.codLibro = codLibro;
        this.titolo = titolo;
        this.autore = autore;
        this.dataInizio = dataInizio;
        this.dataFine = dataFine;
    }

    // Costruttore vuoto per casi in cui i campi vengono settati dopo (es. LibrarianView)
    public Prestito() {}

    // 🔹 Getter e Setter

    public int getCodPrestito() {
        return codPrestito;
    }

    public void setCodPrestito(int codPrestito) {
        this.codPrestito = codPrestito;
    }

    public int getCodLibro() {
        return codLibro;
    }

    public void setCodLibro(int codLibro) {
        this.codLibro = codLibro;
    }

    public String getTitolo() {
        return titolo;
    }

    public void setTitolo(String titolo) {
        this.titolo = titolo;
    }

    public String getAutore() {
        return autore;
    }

    public void setAutore(String autore) {
        this.autore = autore;
    }

    public String getDataInizio() {
        return dataInizio;
    }

    public void setDataInizio(String dataInizio) {
        this.dataInizio = dataInizio;
    }

    public String getDataFine() {
        return dataFine;
    }

    public void setDataFine(String dataFine) {
        this.dataFine = dataFine;
    }

    public String getUtente() {
        return utente;
    }

    public void setUtente(String utente) {
        this.utente = utente;
    }

      public int getCopieDisponibili() {
        return copieDisponibili;
    }

    public void setCopieDisponibili(int copieDisponibili) {
        this.copieDisponibili = copieDisponibili;
    }

    public int getCopieInPrestito() {
        return copieInPrestito;
    }

    public void setCopieInPrestito(int copieInPrestito) {
        this.copieInPrestito = copieInPrestito;
    }

    // 🔹 Metodi di utilità

    /**
     * Richiede al server l’utente associato a questo prestito.
     */
    public String fetchUtenteFromServer(Socket socket) throws IOException {
        PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
        BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));

        out.println("GET_UTENTE_PRESTITO " + codPrestito);

        String response = in.readLine();
        if (response != null && response.startsWith("OK;")) {
            this.utente = response.substring(3);
            return this.utente;
        }
        return null;
    }

    /**
     * Restituisce lo stato del prestito (Valido / In ritardo)
     */
    public String getStato() {
        try {
            LocalDate fine = LocalDate.parse(dataFine);
            return fine.isBefore(LocalDate.now()) ? "In ritardo" : "Valido";
        } catch (Exception e) {
            return "—";
        }
    }

    @Override
    public String toString() {
        return String.format("Prestito[codLibro=%d, titolo=%s, disponibili=%d, inPrestito=%d]",
                codLibro, titolo, copieDisponibili, copieInPrestito);
    }
}
