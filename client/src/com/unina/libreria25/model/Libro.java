package com.unina.libreria25.model;

public class Libro {
    // Il codice della classe Libro rimane invariato
    private final int codLibro;
    private final String titolo;
    private final String autore;
    private final int copie;

    public Libro(int codLibro, String titolo, String autore, int copie) {
        this.codLibro = codLibro;
        this.titolo = titolo;
        this.autore = autore;
        this.copie = copie;
    }

    // Metodi getter... al momento inutilizzati
    public int getCodLibro() {
        return codLibro;
    }

    public String getTitolo() {
        return titolo;
    }

    public String getAutore() {
        return autore;
    }

    public int getCopie() {
        return copie;
    }

    // nella classe com.unina.libreria25.model.Libro
    @Override
    public boolean equals(Object o) {
        if (this == o)
            return true;
        if (!(o instanceof Libro))
            return false;
        Libro other = (Libro) o;
        return this.codLibro == other.codLibro;
    }

    @Override
    public int hashCode() {
        return Integer.hashCode(codLibro);
    }

}