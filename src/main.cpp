#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Window.H>

const int INITIAL_WINDOW_WIDTH = 400;
const int INITIAL_WINDOW_HEIGHT = 300;

int main(int argc, char** argv) {
    Fl_Window* window = new Fl_Window(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Discove");

    Fl_Box* content = new Fl_Box(0, 0, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);
    content->box(FL_UP_BOX);
    content->labelsize(24);
    content->labelfont(FL_BOLD);
    window->resizable(content);

    window->end();
    window->show(argc, argv);

    return Fl::run();
}