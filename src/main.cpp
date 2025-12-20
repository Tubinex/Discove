#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Window.H>

#include <string>

#include "state/Store.h"

const int INITIAL_WINDOW_WIDTH = 1280;
const int INITIAL_WINDOW_HEIGHT = 720;

int main(int argc, char **argv) {
    Fl_Window *window = new Fl_Window(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Discove");
    Fl_Box *counterLabel = new Fl_Box(INITIAL_WINDOW_WIDTH / 2 - 100, 200, 200, 100, "Counter: 0");
    counterLabel->labelsize(32);
    counterLabel->labelfont(FL_BOLD);

    Fl_Button *incrementBtn = new Fl_Button(INITIAL_WINDOW_WIDTH / 2 - 75, 320, 150, 40, "Increment");
    incrementBtn->callback([](Fl_Widget *, void *) { Store::get().update([](AppState &state) { state.counter++; }); });

    Store::get().subscribe<int>([](const AppState &state) { return state.counter; },
                                [counterLabel](int newCounter) {
                                    std::string label = "Counter: " + std::to_string(newCounter);
                                    counterLabel->copy_label(label.c_str());
                                    counterLabel->parent()->redraw();
                                },
                                std::equal_to<int>{}, true);

    window->resizable(window);
    window->size_range(400, 300); 
    window->end();
    window->show(argc, argv);

    return Fl::run();
}